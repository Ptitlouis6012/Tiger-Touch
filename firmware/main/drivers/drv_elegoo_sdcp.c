/* Elegoo Centauri Carbon 1, through SDCP v3: a plain WebSocket on port 3030
 * carrying JSON, no authentication (reference: Tiger Studio's elegoo/sdcp.js,
 * and the protocol report written from a CC1 on firmware V1.4.49).
 *
 * The CC2 of the same brand speaks MQTT and has its own driver (drv_elegoo.c);
 * what tells them apart is the `protocol: "sdcp"` Studio writes with the printer.
 *
 * Every message needs the printer's MainboardID. It comes with the account when
 * Studio found the printer by discovery; when it does not, it is asked the way
 * Studio asks: the text "M99999" to UDP 3000, answered with a JSON that holds
 * it. That exchange is done here without blocking (one datagram a few seconds
 * apart, read on the next tick), so a printer that is off simply stays "not
 * answering" instead of holding the feed up.
 *
 * The printer pushes its status by itself every couple of seconds; one is also
 * asked for every few seconds so that a dead connection is noticed (a send on
 * it fails and the client reconnects). WebSocket pings are left to the library
 * but their absence of answer is not allowed to drop the connection: only the
 * plain-text "ping"/"pong" is documented for this printer.
 *
 * A read-only session, seen by a third party on firmware V1.4.49, covered
 * discovery, the unauthenticated connection and commands 0 (status) and 1
 * (attributes). Pause (129), resume (131) and stop (130) are sent on a touch
 * (sdcp_control); their numbers come from the OpenCentauri API notes, not from a
 * CC1 session. This driver has NOT been run against a real CC1 yet. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "addr.h"
#include "cJSON.h"
#include "driver_util.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "ws_rx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "printer_driver.h"

static const char *TAG = "sdcp";

#define WS_PORT 3030
#define DISCOVERY_PORT 3000
#define MAX_FRAME (16 * 1024)
#define PROBE_EVERY_US (3LL * 1000 * 1000)
#define ASK_EVERY_US (5LL * 1000 * 1000)
#define PING_EVERY_US (10LL * 1000 * 1000)
/* The printer pushes about every 2 s; this is several of them. */
#define ONLINE_WINDOW_US (15LL * 1000 * 1000)

typedef struct
{
    esp_websocket_client_handle_t ws; /* NULL until the MainboardID is known */
    SemaphoreHandle_t lock;
    char ip[40];
    char board[40];
    int udp; /* the discovery socket, -1 when closed */
    int64_t probe_at_us;
    int64_t ask_at_us;
    int64_t ping_at_us;
    int64_t last_msg_us;

    ws_rx_t rx; /* reassembly of a frame handed over in chunks */

    bool connected;
    bool got; /* a status has been read since the connection */

    /* PrintInfo / CurrentStatus, as received */
    bool st_printing;    /* CurrentStatus holds 1 */
    bool st_busy;        /* 3: calibrating */
    int sub;             /* PrintInfo.Status */
    char file[48];
    int progress;        /* PrintInfo.Progress, -1 when absent */
    int layer, total_layer;
    int ticks, total_ticks; /* seconds */
    float nozzle, nozzle_target;
    float bed, bed_target;
    float box;
} sdcp_t;

/* ---- JSON helpers --------------------------------------------------------- */

/* ---- Messages ------------------------------------------------------------- */

static void apply_status(sdcp_t *c, const cJSON *st)
{
    xSemaphoreTake(c->lock, portMAX_DELAY);

    /* CurrentStatus is an array on this firmware; a lone number is tolerated. */
    const cJSON *cs = cJSON_GetObjectItem(st, "CurrentStatus");
    if (cJSON_IsArray(cs) || cJSON_IsNumber(cs))
    {
        c->st_printing = c->st_busy = false;
        if (cJSON_IsNumber(cs))
        {
            c->st_printing = cs->valueint == 1;
            c->st_busy = cs->valueint == 3;
        }
        else
        {
            const cJSON *e;
            cJSON_ArrayForEach(e, cs)
            {
                if (cJSON_IsNumber(e))
                {
                    c->st_printing |= e->valueint == 1;
                    c->st_busy |= e->valueint == 3;
                }
            }
        }
    }

    json_get_f(st, "TempOfNozzle", &c->nozzle);
    json_get_f(st, "TempTargetNozzle", &c->nozzle_target);
    json_get_f(st, "TempOfHotbed", &c->bed);
    json_get_f(st, "TempTargetHotbed", &c->bed_target);
    json_get_f(st, "TempOfBox", &c->box);

    const cJSON *pi = cJSON_GetObjectItem(st, "PrintInfo");
    if (cJSON_IsObject(pi))
    {
        json_get_i(pi, "Status", &c->sub);
        json_get_i(pi, "CurrentLayer", &c->layer);
        json_get_i(pi, "TotalLayer", &c->total_layer);
        json_get_i(pi, "CurrentTicks", &c->ticks);
        json_get_i(pi, "TotalTicks", &c->total_ticks);
        c->progress = -1;
        json_get_i(pi, "Progress", &c->progress);
        const cJSON *fn = cJSON_GetObjectItem(pi, "Filename");
        if (cJSON_IsString(fn))
        {
            const char *name = fn->valuestring;
            const char *slash = strrchr(name, '/');
            strlcpy(c->file, slash ? slash + 1 : name, sizeof c->file);
            char *dot = strrchr(c->file, '.');
            if (dot && strcasecmp(dot, ".gcode") == 0)
            {
                *dot = '\0';
            }
        }
    }
    c->got = true;
    xSemaphoreGive(c->lock);
}

static void handle_frame(sdcp_t *c, const char *data, size_t len)
{
    xSemaphoreTake(c->lock, portMAX_DELAY);
    c->last_msg_us = esp_timer_get_time(); /* a "pong" is proof of life too */
    xSemaphoreGive(c->lock);

    if (len < 2 || data[0] != '{')
    {
        return;
    }
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root)
    {
        return;
    }
    /* sdcp/status/<id> carries "Status" at the top; the answer to a request is
     * sdcp/response/<id> and holds only an Ack. */
    const cJSON *st = cJSON_GetObjectItem(root, "Status");
    if (cJSON_IsObject(st))
    {
        apply_status(c, st);
    }
    cJSON_Delete(root);
}

/* ---- Requests ------------------------------------------------------------- */

static void hex32(char *out)
{
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < 32; i += 8)
    {
        const uint32_t r = esp_random();
        for (int k = 0; k < 8; k++)
        {
            out[i + k] = H[(r >> (k * 4)) & 0xf];
        }
    }
    out[32] = '\0';
}

/* Cmd 0 = "send me your status". */
static void ask_status(sdcp_t *c)
{
    if (!c->ws || !esp_websocket_client_is_connected(c->ws))
    {
        return;
    }
    char id[33], rid[33], msg[400];
    hex32(id);
    hex32(rid);
    const int n = snprintf(msg, sizeof msg,
                           "{\"Id\":\"%s\",\"Data\":{\"Cmd\":0,\"Data\":{},\"RequestID\":\"%s\","
                           "\"MainboardID\":\"%s\",\"TimeStamp\":%lld,\"From\":0},"
                           "\"Topic\":\"sdcp/request/%s\"}",
                           id, rid, c->board, (long long)time(NULL), c->board);
    if (n > 0 && n < (int)sizeof msg)
    {
        esp_websocket_client_send_text(c->ws, msg, n, pdMS_TO_TICKS(300));
    }
}

/* ---- WebSocket ------------------------------------------------------------ */

static void on_ws(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)base;
    sdcp_t *c = (sdcp_t *)arg;
    esp_websocket_event_data_t *d = (esp_websocket_event_data_t *)event_data;

    switch (id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = true;
        c->ask_at_us = 0; /* ask at once */
        xSemaphoreGive(c->lock);
        ask_status(c);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGW(TAG, "disconnected");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = false;
        c->got = false;
        xSemaphoreGive(c->lock);
        break;

    case WEBSOCKET_EVENT_DATA:
    {
        const char *text;
        size_t len;
        if (ws_rx_feed(&c->rx, d, MAX_FRAME, &text, &len))
        {
            handle_frame(c, text, len);
        }
        break;
    }

    default:
        break;
    }
}

static bool open_ws(sdcp_t *c)
{
    char uri[80], hp[ADDR_HOSTPORT_MAX];
    addr_with_port(hp, sizeof hp, c->ip, WS_PORT);
    snprintf(uri, sizeof uri, "ws://%s/websocket", hp);
    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = 2048,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 8000,
        /* 4 KB: measured 2.2 KB used at worst on the bench (docs/CONNECTION-BUDGET.md);
         * the stack is in internal RAM, which is what printers are budgeted in. */
        .task_stack = 4 * 1024,
        /* Core 0, with the rest of the network: core 1 draws the screen. */
        .task_core_id_set = true,
        .task_core_id = 0,
        .disable_pingpong_discon = true,
    };
    c->ws = esp_websocket_client_init(&cfg);
    if (!c->ws)
    {
        return false;
    }
    esp_websocket_register_events(c->ws, WEBSOCKET_EVENT_ANY, on_ws, c);
    esp_websocket_client_start(c->ws);
    ESP_LOGI(TAG, "connecting to the printer");
    return true;
}

/* ---- Discovery of the MainboardID, without blocking ------------------------ */

static void close_udp(sdcp_t *c)
{
    if (c->udp >= 0)
    {
        close(c->udp);
        c->udp = -1;
    }
}

/* Called every second while the id is unknown. */
static void discover_step(sdcp_t *c, int64_t now)
{
    if (c->udp < 0)
    {
        c->udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (c->udp < 0)
        {
            return;
        }
        fcntl(c->udp, F_SETFL, fcntl(c->udp, F_GETFL, 0) | O_NONBLOCK);
    }

    struct sockaddr_in to = {0};
    to.sin_family = AF_INET;
    to.sin_port = htons(DISCOVERY_PORT);
    char host[ADDR_HOSTPORT_MAX];
    addr_host_only(host, sizeof host, c->ip); /* a port in the address is the WebSocket's; discovery stays on its own */
    if (inet_pton(AF_INET, host, &to.sin_addr) != 1)
    {
        return; /* not an address: nothing to ask */
    }

    /* The answer to the last question, if it came. */
    char buf[512];
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    const int n = recvfrom(c->udp, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
    if (n > 0 && from.sin_addr.s_addr == to.sin_addr.s_addr)
    {
        buf[n] = '\0';
        cJSON *r = cJSON_Parse(buf);
        const cJSON *data = r ? cJSON_GetObjectItem(r, "Data") : NULL;
        const cJSON *id = cJSON_IsObject(data) ? cJSON_GetObjectItem(data, "MainboardID") : NULL;
        if (cJSON_IsString(id) && id->valuestring[0])
        {
            xSemaphoreTake(c->lock, portMAX_DELAY);
            strlcpy(c->board, id->valuestring, sizeof c->board);
            xSemaphoreGive(c->lock);
        }
        cJSON_Delete(r);
        if (c->board[0])
        {
            close_udp(c);
            open_ws(c);
            return;
        }
    }

    if (now >= c->probe_at_us)
    {
        c->probe_at_us = now + PROBE_EVERY_US;
        sendto(c->udp, "M99999", 6, 0, (struct sockaddr *)&to, sizeof to);
    }
}

/* ---- Driver --------------------------------------------------------------- */

static drv_kind_t sdcp_support(const account_printer_t *p)
{
    return (p->ip[0] && !p->cloud && strcmp(p->proto, "sdcp") == 0) ? DRV_LAN : DRV_NONE;
}

static void *sdcp_start(const account_printer_t *p)
{
    sdcp_t *c = calloc(1, sizeof *c);
    if (!c)
    {
        return NULL;
    }
    c->lock = xSemaphoreCreateMutex();
    c->udp = -1;
    c->progress = -1;
    strlcpy(c->ip, p->ip, sizeof c->ip);
    strlcpy(c->board, p->board_id, sizeof c->board);
    if (c->board[0] && !open_ws(c))
    {
        vSemaphoreDelete(c->lock);
        free(c);
        return NULL;
    }
    return c;
}

static void sdcp_stop(void *ctx)
{
    sdcp_t *c = (sdcp_t *)ctx;
    if (!c)
    {
        return;
    }
    if (c->ws)
    {
        esp_websocket_client_stop(c->ws);
        esp_websocket_client_destroy(c->ws);
    }
    close_udp(c);
    vSemaphoreDelete(c->lock);
    ws_rx_free(&c->rx);
    free(c);
}

/* Pause, resume, stop: SDCP commands 129 (pause), 131 (resume) and 130 (stop); the numbers come from the OpenCentauri API notes, which Studio itself flags as not confirmed on a CC1. Written from Tiger Studio's protocol notes; this driver
 * has not been run against a real printer, so the command is not verified. */
static bool sdcp_control(void *ctx, pv_control_t cmd)
{
    static const int CMD[] = {[PV_CTL_PAUSE] = 129, [PV_CTL_RESUME] = 131, [PV_CTL_STOP] = 130};
    sdcp_t *c = (sdcp_t *)ctx;
    if (!c || !c->ws || !c->board[0] || !esp_websocket_client_is_connected(c->ws) || (int)cmd < 0 ||
        (int)cmd > PV_CTL_STOP)
    {
        return false;
    }
    char id[33], rid[33], msg[400];
    hex32(id);
    hex32(rid);
    const int n = snprintf(msg, sizeof msg,
                           "{\"Id\":\"%s\",\"Data\":{\"Cmd\":%d,\"Data\":{},\"RequestID\":\"%s\","
                           "\"MainboardID\":\"%s\",\"TimeStamp\":%lld,\"From\":0},"
                           "\"Topic\":\"sdcp/request/%s\"}",
                           id, CMD[cmd], rid, c->board, (long long)time(NULL), c->board);
    return n > 0 && n < (int)sizeof msg && esp_websocket_client_send_text(c->ws, msg, n, pdMS_TO_TICKS(300)) >= 0;
}

static void sdcp_snapshot(void *ctx, printer_view_t *v)
{
    sdcp_t *c = (sdcp_t *)ctx;
    const int64_t now = esp_timer_get_time();

    if (!c->ws)
    {
        discover_step(c, now);
    }
    else if (c->connected)
    {
        if (now >= c->ask_at_us)
        {
            c->ask_at_us = now + ASK_EVERY_US;
            ask_status(c);
        }
        if (now >= c->ping_at_us)
        {
            c->ping_at_us = now + PING_EVERY_US;
            esp_websocket_client_send_text(c->ws, "ping", 4, pdMS_TO_TICKS(300));
        }
    }

    xSemaphoreTake(c->lock, portMAX_DELAY);
    v->online = c->connected && c->got && c->last_msg_us != 0 && (now - c->last_msg_us) < ONLINE_WINDOW_US;
    v->can_control = v->online;

    /* PrintInfo.Status: 5 pausing, 6 paused, 7 stopping, 8 stopped, 9 complete.
     * While idle the printer keeps the LAST job's totals with Status 8 and an
     * empty Filename, so "a job" is decided from CurrentStatus and the file,
     * never from the totals. */
    bool running = false;
    if (c->st_printing)
    {
        if (c->sub == 5 || c->sub == 6)
        {
            v->state = PV_PAUSED;
            running = true;
        }
        else if (c->sub == 7)
        {
            v->state = PV_IDLE; /* being stopped */
        }
        else
        {
            v->state = PV_PRINTING;
            running = true;
        }
    }
    else if (c->st_busy)
    {
        v->state = PV_PRINTING; /* calibrating before a print */
    }
    else if (c->file[0] && c->sub == 9)
    {
        v->state = PV_DONE;
    }
    else
    {
        v->state = PV_IDLE;
    }

    int pct = 0;
    if (v->state == PV_DONE)
    {
        pct = 100;
    }
    else if (running)
    {
        pct = c->progress >= 0 ? c->progress
                               : (c->total_layer > 0 ? c->layer * 100 / c->total_layer : 0);
    }
    v->progress = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    v->layer = running ? c->layer : 0;
    v->total_layer = running ? c->total_layer : 0;
    v->left_s = (running && c->total_ticks > c->ticks) ? c->total_ticks - c->ticks : 0;
    v->nozzle = c->nozzle;
    v->nozzle_target = c->nozzle_target;
    v->bed = c->bed;
    v->bed_target = c->bed_target;
    v->chamber = c->box;
    strlcpy(v->file, (running || v->state == PV_DONE) ? c->file : "", sizeof v->file);

    /* The Canvas, when there is one, does not expose its slots over SDCP. */
    memset(v->unit, 0, sizeof v->unit);
    xSemaphoreGive(c->lock);
}

const printer_driver_t DRIVER_ELEGOO_SDCP = {
    .brand = "elegoo",
    .support = sdcp_support,
    .start = sdcp_start,
    .stop = sdcp_stop,
    .snapshot = sdcp_snapshot,
    .control = sdcp_control,
};
