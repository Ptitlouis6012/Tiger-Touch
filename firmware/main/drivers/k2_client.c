#include "k2_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "addr.h"
#include "cJSON.h"
#include "driver_util.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "ws_rx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "k2";

#define MAX_FRAME (24 * 1024)
#define BOXS_REFRESH_US (30LL * 1000 * 1000)

/* `state`: 0 rest, 1 printing, 2 finished (Tiger Studio's notes), 5 paused (seen on a K2). */
#define K2_STATE_PAUSED 5

struct k2_client
{
    esp_websocket_client_handle_t ws;
    SemaphoreHandle_t lock;
    char ip[40];

    /* frame reassembly (the client hands payloads over in chunks) */
    ws_rx_t rx;

    bool connected;
    bool got_frame;
    int64_t last_boxs_req_us;

    float nozzle, nozzle_target, bed, bed_target, chamber;
    int state, progress, layer, total_layer, left_s;
    int paused;                /* the printer's own pause flag */
    int err_code;              /* 0 when there is no fault */
    bool light_on;             /* `lightSw`: 1 is the light on */
    int last_layer, last_total; /* kept: the K2 zeroes them when a print ends */
    char file[48];
    pv_unit_t unit[PV_MAX_UNITS];
};

/* ---- JSON helpers --------------------------------------------------------- */

/* Creality sends many numbers as strings. */
static bool as_float(const cJSON *it, float *out)
{
    if (cJSON_IsNumber(it))
    {
        *out = (float)it->valuedouble;
        return true;
    }
    if (cJSON_IsString(it) && it->valuestring[0])
    {
        *out = (float)atof(it->valuestring);
        return true;
    }
    return false;
}

static bool as_int(const cJSON *it, int *out)
{
    float f;
    if (!as_float(it, &f))
    {
        return false;
    }
    *out = (int)(f + (f >= 0 ? 0.5f : -0.5f));
    return true;
}

/* ---- Frame handling ------------------------------------------------------- */

static void apply_boxs(k2_client_t *c, const cJSON *boxs)
{
    pv_unit_t units[PV_MAX_UNITS];
    memset(units, 0, sizeof units);

    /* Every CFS box becomes a unit (type 0 = CFS, 1 = the external spool). */
    int cfs_total = 0;
    const cJSON *box;
    cJSON_ArrayForEach(box, cJSON_GetObjectItem(boxs, "materialBoxs"))
    {
        const cJSON *type = cJSON_GetObjectItem(box, "type");
        if (cJSON_IsNumber(type) && type->valueint == 0)
        {
            cfs_total++;
        }
    }
    int n = 0;
    cJSON_ArrayForEach(box, cJSON_GetObjectItem(boxs, "materialBoxs"))
    {
        const cJSON *btype = cJSON_GetObjectItem(box, "type");
        if (!cJSON_IsNumber(btype) || btype->valueint != 0 || n >= PV_MAX_UNITS)
        {
            continue;
        }
        pv_unit_t *u = &units[n];
        u->present = true;
        u->slots = 4;
        if (cfs_total > 1)
        {
            snprintf(u->label, sizeof u->label, "CFS %d", n + 1);
        }
        else
        {
            snprintf(u->label, sizeof u->label, "CFS");
        }
        const cJSON *m;
        cJSON_ArrayForEach(m, cJSON_GetObjectItem(box, "materials"))
        {
            int id = -1;
            if (!as_int(cJSON_GetObjectItem(m, "id"), &id) || id < 0 || id > 3)
            {
                continue;
            }
            int st = 0, pct = 0, sel = 0;
            as_int(cJSON_GetObjectItem(m, "state"), &st);
            as_int(cJSON_GetObjectItem(m, "percent"), &pct);
            as_int(cJSON_GetObjectItem(m, "selected"), &sel);
            const cJSON *type = cJSON_GetObjectItem(m, "type");
            const cJSON *color = cJSON_GetObjectItem(m, "color");
            const cJSON *nm = cJSON_GetObjectItem(m, "name");
            pv_slot_t *s = &u->slot[id];
            s->present = st != 0 || (cJSON_IsString(type) && type->valuestring[0]);
            s->percent = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
            s->selected = sel != 0;
            if (cJSON_IsString(type) && type->valuestring[0])
            {
                strlcpy(s->material, type->valuestring, sizeof s->material);
            }
            else if (cJSON_IsString(nm))
            {
                strlcpy(s->material, nm->valuestring, sizeof s->material);
            }
            s->color = color_from_hex(cJSON_IsString(color) ? color->valuestring : NULL);
        }
        n++;
    }
    /* The external spool (type 1) is a unit of its own, after the boxes. */
    if (n < PV_MAX_UNITS)
    {
        cJSON_ArrayForEach(box, cJSON_GetObjectItem(boxs, "materialBoxs"))
        {
            const cJSON *bt = cJSON_GetObjectItem(box, "type");
            if (!cJSON_IsNumber(bt) || bt->valueint != 1)
            {
                continue;
            }
            const cJSON *m = cJSON_GetArrayItem(cJSON_GetObjectItem(box, "materials"), 0);
            if (!m)
            {
                break;
            }
            pv_unit_t *u = &units[n++];
            u->present = true;
            u->slots = 1;
            snprintf(u->label, sizeof u->label, PV_LABEL_EXT);
            int st = 0, sel = 0;
            as_int(cJSON_GetObjectItem(m, "state"), &st);
            as_int(cJSON_GetObjectItem(m, "selected"), &sel);
            const cJSON *type = cJSON_GetObjectItem(m, "type");
            const cJSON *color = cJSON_GetObjectItem(m, "color");
            const cJSON *nm = cJSON_GetObjectItem(m, "name");
            pv_slot_t *s = &u->slot[0];
            /* "no filament" is state 0; the colour of an empty holder is a
             * placeholder, so presence is the state alone here. */
            s->present = st != 0;
            s->selected = sel != 0;
            if (s->present && cJSON_IsString(type) && type->valuestring[0])
            {
                strlcpy(s->material, type->valuestring, sizeof s->material);
            }
            else if (s->present && cJSON_IsString(nm))
            {
                strlcpy(s->material, nm->valuestring, sizeof s->material);
            }
            s->color = color_from_hex(cJSON_IsString(color) ? color->valuestring : NULL);
            break;
        }
    }

    xSemaphoreTake(c->lock, portMAX_DELAY);
    memcpy(c->unit, units, sizeof units);
    xSemaphoreGive(c->lock);
}

static void apply_status(k2_client_t *c, const cJSON *root)
{
    xSemaphoreTake(c->lock, portMAX_DELAY);
    float f;
    int i;
    if (as_float(cJSON_GetObjectItem(root, "nozzleTemp"), &f)) c->nozzle = f;
    if (as_float(cJSON_GetObjectItem(root, "targetNozzleTemp"), &f)) c->nozzle_target = f;
    if (as_float(cJSON_GetObjectItem(root, "bedTemp0"), &f)) c->bed = f;
    if (as_float(cJSON_GetObjectItem(root, "targetBedTemp0"), &f)) c->bed_target = f;
    if (as_float(cJSON_GetObjectItem(root, "boxTemp"), &f)) c->chamber = f;
    if (as_int(cJSON_GetObjectItem(root, "state"), &i)) c->state = i;
    if (as_int(cJSON_GetObjectItem(root, "printProgress"), &i)) c->progress = i;
    if (as_int(cJSON_GetObjectItem(root, "layer"), &i)) c->layer = i;
    if (as_int(cJSON_GetObjectItem(root, "TotalLayer"), &i)) c->total_layer = i;
    if (as_int(cJSON_GetObjectItem(root, "pause"), &i) || as_int(cJSON_GetObjectItem(root, "isPaused"), &i))
    {
        c->paused = i != 0;
    }
    const cJSON *er = cJSON_GetObjectItem(root, "err");
    if (cJSON_IsObject(er))
    {
        if (as_int(cJSON_GetObjectItem(er, "errcode"), &i)) c->err_code = i;
    }
    else if (as_int(er, &i))
    {
        c->err_code = i;
    }
    if (as_int(cJSON_GetObjectItem(root, "lightSw"), &i)) c->light_on = i != 0;
    if (c->layer > 0) c->last_layer = c->layer;
    if (c->total_layer > 0) c->last_total = c->total_layer;
    if (as_int(cJSON_GetObjectItem(root, "printLeftTime"), &i)) c->left_s = i;
    const cJSON *fn = cJSON_GetObjectItem(root, "printFileName");
    if (cJSON_IsString(fn))
    {
        const char *name = fn->valuestring;
        const char *slash = strrchr(name, '/');
        if (slash)
        {
            name = slash + 1;
        }
        strlcpy(c->file, name, sizeof c->file);
        char *dot = strrchr(c->file, '.');
        if (dot && strcasecmp(dot, ".gcode") == 0)
        {
            *dot = '\0';
        }
    }
    c->got_frame = true;
    xSemaphoreGive(c->lock);
}

static void handle_frame(k2_client_t *c, const char *data, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root)
    {
        return;
    }
    const cJSON *mode = cJSON_GetObjectItem(root, "ModeCode");
    if (cJSON_IsString(mode) && strcmp(mode->valuestring, "heart_beat") == 0)
    {
        /* Plain text, not JSON; without it the printer drops the link. */
        esp_websocket_client_send_text(c->ws, "ok", 2, pdMS_TO_TICKS(1000));
    }
    else
    {
        const cJSON *boxs = cJSON_GetObjectItem(root, "boxsInfo");
        if (cJSON_IsObject(boxs))
        {
            apply_boxs(c, boxs);
        }
        apply_status(c, root);
    }
    cJSON_Delete(root);
}

static void request_boxs(k2_client_t *c)
{
    static const char REQ[] = "{\"method\":\"get\",\"params\":{\"boxsInfo\":1}}";
    if (c->ws && esp_websocket_client_is_connected(c->ws))
    {
        esp_websocket_client_send_text(c->ws, REQ, sizeof REQ - 1, pdMS_TO_TICKS(1000));
        c->last_boxs_req_us = esp_timer_get_time();
    }
}

static void on_ws(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)base;
    k2_client_t *c = (k2_client_t *)arg;
    esp_websocket_event_data_t *d = (esp_websocket_event_data_t *)event_data;

    switch (id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = true;
        xSemaphoreGive(c->lock);
        request_boxs(c);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGW(TAG, "disconnected");
        xSemaphoreTake(c->lock, portMAX_DELAY);
        c->connected = false;
        c->got_frame = false;
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

/* ---- Public API ----------------------------------------------------------- */

k2_client_t *k2_client_start(const char *ip)
{
    k2_client_t *c = calloc(1, sizeof *c);
    if (!c)
    {
        return NULL;
    }
    c->lock = xSemaphoreCreateMutex();
    strlcpy(c->ip, ip, sizeof c->ip);

    char uri[64], hp[ADDR_HOSTPORT_MAX];
    addr_with_port(hp, sizeof hp, ip, 9999);
    snprintf(uri, sizeof uri, "ws://%s", hp);
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
        .ping_interval_sec = 5,    /* a printer that vanished without closing is noticed */
        .pingpong_timeout_sec = 10,
    };
    c->ws = esp_websocket_client_init(&cfg);
    if (!c->ws)
    {
        vSemaphoreDelete(c->lock);
        free(c);
        return NULL;
    }
    esp_websocket_register_events(c->ws, WEBSOCKET_EVENT_ANY, on_ws, c);
    esp_websocket_client_start(c->ws);
    ESP_LOGI(TAG, "connecting to the printer");
    return c;
}

void k2_client_stop(k2_client_t *c)
{
    if (!c)
    {
        return;
    }
    esp_websocket_client_stop(c->ws);
    esp_websocket_client_destroy(c->ws);
    vSemaphoreDelete(c->lock);
    ws_rx_free(&c->rx);
    free(c);
}

bool k2_client_control(k2_client_t *c, pv_control_t cmd)
{
    static const char *const BODY[] = {
        [PV_CTL_PAUSE] = "{\"method\":\"set\",\"params\":{\"pause\":1}}",
        [PV_CTL_RESUME] = "{\"method\":\"set\",\"params\":{\"pause\":0}}",
        [PV_CTL_STOP] = "{\"method\":\"set\",\"params\":{\"stop\":1}}",
        [PV_CTL_LIGHT_ON] = "{\"method\":\"set\",\"params\":{\"lightSw\":1}}",
        [PV_CTL_LIGHT_OFF] = "{\"method\":\"set\",\"params\":{\"lightSw\":0}}",
    };
    if (!c || !c->ws || !c->connected || (int)cmd < 0 || (int)cmd > PV_CTL_LIGHT_OFF)
    {
        return false;
    }
    return esp_websocket_client_send_text(c->ws, BODY[cmd], (int)strlen(BODY[cmd]), pdMS_TO_TICKS(1000)) >= 0;
}

void k2_client_snapshot(k2_client_t *c, printer_view_t *v)
{
    /* The boxes are not pushed: ask again from time to time. */
    if (c->connected && esp_timer_get_time() - c->last_boxs_req_us > BOXS_REFRESH_US)
    {
        request_boxs(c);
    }

    xSemaphoreTake(c->lock, portMAX_DELAY);
    v->online = c->connected && c->got_frame;
    v->can_control = v->online;
    v->can_light = v->online;
    v->light_on = c->light_on;
    /* `state` is 0 at rest, 1 while printing, 2 once finished, 5 while paused
     * (seen on a K2); a fault is reported by `err` (Tiger Studio's
     * creality/PROTOCOL.md, section 8). */
    const bool finished = c->progress >= 100 || c->state == 2;
    if (c->err_code != 0)
    {
        v->state = PV_ERROR;
    }
    else if (finished)
    {
        v->state = PV_DONE;
    }
    else if (c->state == 1 || c->state == K2_STATE_PAUSED || c->progress > 0 || c->left_s > 0 || c->layer > 0)
    {
        /* Paused is a `pause` / `isPaused` flag in Tiger Studio's notes, but the K2
         * was seen reporting it as `state` 5 (no flag at all) while paused. */
        v->state = (c->paused || c->state == K2_STATE_PAUSED) ? PV_PAUSED : PV_PRINTING;
    }
    else
    {
        v->state = PV_IDLE;
    }
    v->progress = v->state == PV_IDLE ? 0 : (v->state == PV_DONE ? 100 : c->progress);
    v->layer = c->layer;
    v->total_layer = c->total_layer;
    if (v->state == PV_DONE && v->layer == 0)
    {
        v->layer = c->last_layer; /* the K2 zeroes the counters at the end */
        v->total_layer = c->last_total;
    }
    v->left_s = (v->state == PV_PRINTING || v->state == PV_PAUSED) ? c->left_s : 0;
    v->nozzle = c->nozzle;
    v->nozzle_target = c->nozzle_target;
    v->bed = c->bed;
    v->bed_target = c->bed_target;
    v->chamber = c->chamber;
    strlcpy(v->file, c->file, sizeof v->file);
    memcpy(v->unit, c->unit, sizeof v->unit);
    xSemaphoreGive(c->lock);
}
