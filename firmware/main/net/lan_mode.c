#include "lan_mode.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/idf_additions.h"
#include "lvgl.h"
#include "pandatouch_display.h"
#include "settings.h"

static const char *TAG = "lan";

/* ---- The code and the switch ------------------------------------------------- */

#define CODE_LEN 6
/* Capitals and digits, no look-alikes (no I, O, 0 or 1): 32 characters, 30 bits.
 * Typed in lower case, it is still taken. */
static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
#define ALPHABET_N ((int)sizeof ALPHABET - 1)

static char s_code[CODE_LEN + 1];
static bool s_live;
static uint32_t s_version;
static httpd_handle_t s_srv;

static void draw_code(void)
{
    for (int i = 0; i < CODE_LEN;)
    {
        /* Rejection above the largest multiple of the alphabet's size, so
         * that every character is as likely as any other. */
        const uint32_t r = esp_random() & 0xFF;
        if (r < (256 / ALPHABET_N) * ALPHABET_N)
        {
            s_code[i++] = ALPHABET[r % ALPHABET_N];
        }
    }
    s_code[CODE_LEN] = '\0';
    settings_set("lan_code", s_code);
}

static bool code_valid(const char *c)
{
    if (strlen(c) != CODE_LEN)
    {
        return false;
    }
    for (int i = 0; i < CODE_LEN; i++)
    {
        if (!strchr(ALPHABET, c[i]))
        {
            return false;
        }
    }
    return true;
}

const char *lan_mode_code(void)
{
    return s_code;
}

bool lan_mode_live(void)
{
    return s_live;
}

uint32_t lan_mode_version(void)
{
    return s_version;
}

void lan_mode_new_code(void)
{
    draw_code();
    s_version++;
    ESP_LOGI(TAG, "a new access code was drawn");
}

/* ---- Who may ask ------------------------------------------------------------- */

#define FAILS_MAX 10
#define LOCK_US (60LL * 1000 * 1000)

static int s_fails;
static int64_t s_locked_until;

static int qint(httpd_req_t *req, const char *key, int dflt)
{
    char q[160], v[16];
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK && httpd_query_key_value(q, key, v, sizeof v) == ESP_OK)
    {
        return atoi(v);
    }
    return dflt;
}

/* The code in `c`, compared in constant time. Answers 403 (wrong) or 429
 * (locked) itself and returns false. */
static bool allowed(httpd_req_t *req)
{
    const int64_t now = esp_timer_get_time();
    if (now < s_locked_until)
    {
        httpd_resp_set_status(req, "429 Too Many Requests");
        httpd_resp_sendstr(req, "too many wrong codes: wait a minute\n");
        return false;
    }
    char q[160], c[16] = "";
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK)
    {
        httpd_query_key_value(q, "c", c, sizeof c);
    }
    uint8_t diff = strlen(c) == CODE_LEN ? 0 : 1;
    for (int i = 0; i < CODE_LEN; i++)
    {
        diff |= (uint8_t)(toupper((unsigned char)c[i]) ^ s_code[i]);
    }
    /* Tiger Studio calls from its own page: the code is the gate, not the origin. */
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (diff == 0)
    {
        s_fails = 0;
        return true;
    }
    if (++s_fails >= FAILS_MAX)
    {
        s_fails = 0;
        s_locked_until = now + LOCK_US;
        ESP_LOGW(TAG, "%d wrong access codes: nothing accepted for a minute", FAILS_MAX);
    }
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_sendstr(req, "wrong access code\n");
    return false;
}

/* ---- The pointer, the frame counter, what changed -------------------------------- */

static volatile int s_x, s_y;
static volatile bool s_pressed;
static lv_indev_t *s_indev;

/* Each frame LVGL draws moves the counter, and the union of the areas it was
 * asked to redraw is kept for the last HIST frames: a client that holds frame
 * N is sent only what changed since. Written by the LVGL task; read under the
 * LVGL lock. */
#define HIST 64
static volatile uint32_t s_frame;
static volatile int64_t s_frame_us;
static lv_area_t s_pend, s_hist[HIST];
static bool s_pend_any, s_hist_any[HIST];

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = s_x;
    data->point.y = s_y;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void grow(lv_area_t *u, bool *any, const lv_area_t *a)
{
    if (!*any)
    {
        *u = *a;
        *any = true;
        return;
    }
    u->x1 = LV_MIN(u->x1, a->x1);
    u->y1 = LV_MIN(u->y1, a->y1);
    u->x2 = LV_MAX(u->x2, a->x2);
    u->y2 = LV_MAX(u->y2, a->y2);
}

static void on_invalidate(lv_event_t *e)
{
    const lv_area_t *a = lv_event_get_param(e);
    if (a)
    {
        grow(&s_pend, &s_pend_any, a);
    }
}

static void on_render(lv_event_t *e)
{
    (void)e;
    const uint32_t f = s_frame + 1;
    s_hist[f % HIST] = s_pend;
    s_hist_any[f % HIST] = s_pend_any;
    s_pend_any = false;
    s_frame_us = esp_timer_get_time();
    s_frame = f;
}

/* Once: the pointer and the frame counter, kept across a stop and a start. */
static void ensure_hooks(void)
{
    if (s_indev)
    {
        return;
    }
    PT_LVGL_SCOPE_LOCK()
    {
        s_indev = lv_indev_create();
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, read_cb);
        lv_display_t *d = lv_display_get_default();
        lv_display_add_event_cb(d, on_invalidate, LV_EVENT_INVALIDATE_AREA, NULL);
        lv_display_add_event_cb(d, on_render, LV_EVENT_RENDER_READY, NULL);
    }
}

/* A frame after `ver`, then 150 ms with none: the end of an animation, not its
 * middle (a scroll's frames came up to 100 ms apart, measured on this board). */
#define SETTLE_US (150 * 1000)
static bool settled_after(uint32_t ver)
{
    return s_frame != ver && esp_timer_get_time() - s_frame_us > SETTLE_US;
}

static uint32_t wait_settled(uint32_t ver, int ms)
{
    const int64_t end = esp_timer_get_time() + (int64_t)ms * 1000;
    while (!settled_after(ver) && esp_timer_get_time() < end)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return s_frame;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* With `wait=1`, a touch answers once the screen has redrawn after it (1.5 s at
 * most), its frame in X-Frame-Ver: the client fetches that frame at once. */
static void touch_done(httpd_req_t *req, uint32_t before)
{
    char ver[12];
    if (qint(req, "wait", 0))
    {
        snprintf(ver, sizeof ver, "%lu", (unsigned long)wait_settled(before, 1500));
    }
    else
    {
        snprintf(ver, sizeof ver, "%lu", (unsigned long)s_frame);
    }
    httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Frame-Ver");
    httpd_resp_set_hdr(req, "X-Frame-Ver", ver);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
}

static esp_err_t h_tap(httpd_req_t *req)
{
    if (!allowed(req))
    {
        return ESP_OK;
    }
    s_x = clampi(qint(req, "x", 0), 0, 799);
    s_y = clampi(qint(req, "y", 0), 0, 479);
    s_pressed = true;
    vTaskDelay(pdMS_TO_TICKS(120));
    const uint32_t before = s_frame;
    s_pressed = false;
    touch_done(req, before);
    return ESP_OK;
}

static esp_err_t h_swipe(httpd_req_t *req)
{
    if (!allowed(req))
    {
        return ESP_OK;
    }
    const int x1 = clampi(qint(req, "x1", 0), 0, 799), y1 = clampi(qint(req, "y1", 0), 0, 479);
    const int x2 = clampi(qint(req, "x2", 0), 0, 799), y2 = clampi(qint(req, "y2", 0), 0, 479);
    const int ms = clampi(qint(req, "ms", 300), 60, 2000);
    const int steps = ms / 20;
    s_x = x1;
    s_y = y1;
    s_pressed = true;
    vTaskDelay(pdMS_TO_TICKS(60));
    for (int i = 1; i <= steps; i++)
    {
        s_x = x1 + (x2 - x1) * i / steps;
        s_y = y1 + (y2 - y1) * i / steps;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(60));
    const uint32_t before = s_frame;
    s_pressed = false;
    touch_done(req, before);
    return ESP_OK;
}

/* ---- The frame counter, and the long poll ---------------------------------------- */
/* `wait=<ver>&ms=<max>`: held until a frame after <ver> has settled, or <max> ms
 * (5000 at most). The server answers one request at a time, so a held one is
 * handed to a task of its own: a touch sent meanwhile is not kept waiting. */

#define LP_MAX 3
typedef struct
{
    httpd_req_t *req;
    uint32_t wait;
    int64_t end;
} lp_t;
static QueueHandle_t s_lpq;
static volatile bool s_stopping;

static void send_version(httpd_req_t *req)
{
    char b[16];
    snprintf(b, sizeof b, "%lu", (unsigned long)s_frame);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_sendstr(req, b);
}

static void lp_task(void *arg)
{
    (void)arg;
    lp_t held[LP_MAX];
    int n = 0;
    for (;;)
    {
        lp_t in;
        while (n < LP_MAX && xQueueReceive(s_lpq, &in, n ? 0 : portMAX_DELAY) == pdTRUE)
        {
            held[n++] = in;
        }
        const int64_t now = esp_timer_get_time();
        for (int i = 0; i < n;)
        {
            if (s_stopping || now >= held[i].end || settled_after(held[i].wait))
            {
                send_version(held[i].req);
                httpd_req_async_handler_complete(held[i].req);
                held[i] = held[--n];
            }
            else
            {
                i++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static esp_err_t h_version(httpd_req_t *req)
{
    if (!allowed(req))
    {
        return ESP_OK;
    }
    const int wait = qint(req, "wait", -1);
    httpd_req_t *async = NULL;
    if (wait < 0 || (uint32_t)wait != s_frame || !s_lpq || httpd_req_async_handler_begin(req, &async) != ESP_OK)
    {
        send_version(req); /* no wait asked, already moved, or no room: at once */
        return ESP_OK;
    }
    const lp_t lp = {.req = async,
                     .wait = (uint32_t)wait,
                     .end = esp_timer_get_time() + (int64_t)clampi(qint(req, "ms", 5000), 0, 5000) * 1000};
    if (xQueueSend(s_lpq, &lp, 0) != pdTRUE)
    {
        send_version(async);
        httpd_req_async_handler_complete(async);
    }
    return ESP_OK;
}

/* ---- The picture ---------------------------------------------------------------- */

/* The screen as shown, the top layer blended over it, in RGB565. NULL without
 * the memory for it. The frame it shows in `*ver`. */
static lv_draw_buf_t *take_565(uint32_t *ver, lv_area_t *changed, bool *any, uint32_t since, bool want_since)
{
    lv_draw_buf_t *scr = NULL, *top = NULL;
    PT_LVGL_SCOPE_LOCK()
    {
        scr = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
        if (scr && lv_obj_get_child_count(lv_layer_top()) > 0)
        {
            top = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888);
        }
        *ver = s_frame;
        /* What changed after `since`: the frames since, if still in the history. */
        *any = false;
        if (want_since && since <= *ver && *ver - since < HIST)
        {
            for (uint32_t f = since + 1; f <= *ver; f++)
            {
                if (s_hist_any[f % HIST])
                {
                    grow(changed, any, &s_hist[f % HIST]);
                }
            }
            /* Drawn into the picture already, not yet into a frame: sent now too. */
            if (s_pend_any)
            {
                grow(changed, any, &s_pend);
            }
        }
        else
        {
            changed->x1 = 0;
            changed->y1 = 0;
            changed->x2 = 799;
            changed->y2 = 479;
            *any = true;
        }
    }
    if (scr && top)
    {
        for (int y = 0; y < (int)scr->header.h && y < (int)top->header.h; y++)
        {
            uint16_t *d = (uint16_t *)(scr->data + (size_t)y * scr->header.stride);
            const uint8_t *t = top->data + (size_t)y * top->header.stride;
            for (int x = 0; x < (int)scr->header.w && x < (int)top->header.w; x++)
            {
                const uint8_t *p = t + x * 4; /* B G R A */
                const int a = p[3];
                if (a == 0)
                {
                    continue;
                }
                const uint16_t c = d[x];
                const int r = ((c >> 11) & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
                const int R = (p[2] * a + r * (255 - a)) / 255, G = (p[1] * a + g * (255 - a)) / 255,
                          B = (p[0] * a + b * (255 - a)) / 255;
                d[x] = (uint16_t)(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3));
            }
        }
    }
    if (top)
    {
        lv_draw_buf_destroy(top);
    }
    return scr;
}

/* TigerScale V3's run-length code for RGB565 (its live view), so that one
 * decoder reads both: a token byte t, then
 *   t 0x00-0x7F  t+1 literal pixels follow (2 bytes each, little-endian)
 *   t 0x80-0xFE  one pixel follows, repeated t-0x80+2 times
 *   t 0xFF       a u16 count (little-endian) then one pixel, repeated count times
 * The screen is flat colour: a frame is several times smaller. Returns the
 * bytes written; `out` holds 3 bytes a pixel at worst. */
static size_t rle565(const uint16_t *p, int n, uint8_t *out)
{
    size_t o = 0;
    int i = 0;
    while (i < n)
    {
        int r = 1;
        while (i + r < n && p[i + r] == p[i] && r < 65535)
        {
            r++;
        }
        if (r >= 2)
        {
            if (r <= 128)
            {
                out[o++] = (uint8_t)(0x80 + r - 2);
            }
            else
            {
                out[o++] = 0xFF;
                out[o++] = (uint8_t)r;
                out[o++] = (uint8_t)(r >> 8);
            }
            out[o++] = (uint8_t)p[i];
            out[o++] = (uint8_t)(p[i] >> 8);
            i += r;
            continue;
        }
        /* Literals, up to 128, until a run of two begins. */
        int l = 1;
        while (i + l < n && l < 128 && !(i + l + 1 < n && p[i + l] == p[i + l + 1]))
        {
            l++;
        }
        out[o++] = (uint8_t)(l - 1);
        for (int k = 0; k < l; k++)
        {
            out[o++] = (uint8_t)p[i + k];
            out[o++] = (uint8_t)(p[i + k] >> 8);
        }
        i += l;
    }
    return o;
}

/* GET /screen.raw?c=&fmt=rgb565|rle565[&scale=2][&since=<ver>]: the pixels of the
 * rectangle that changed since <ver> (the whole screen without it, or when the
 * history is gone), row-major, little-endian RGB565, halved with scale=2 (each
 * pixel the mean of four). X-Rect is the rectangle in 800x480 coordinates;
 * with scale=2 it is on even coordinates. An empty X-Rect (0,0,0,0) and no
 * body: nothing changed. With fmt=rle565 the same pixels, run-length coded
 * (rle565() above). */
static esp_err_t h_raw(httpd_req_t *req)
{
    if (!allowed(req))
    {
        return ESP_OK;
    }
    char q[160], fmt[12] = "rgb565";
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK)
    {
        httpd_query_key_value(q, "fmt", fmt, sizeof fmt);
    }
    const bool rle = strcmp(fmt, "rle565") == 0;
    if (!rle && strcmp(fmt, "rgb565") != 0)
    {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "fmt: rgb565 or rle565\n");
        return ESP_OK;
    }
    const int scale = qint(req, "scale", 1) == 2 ? 2 : 1;
    const int since = qint(req, "since", -1);
    uint32_t ver = 0;
    lv_area_t r = {0};
    bool any = false;
    lv_draw_buf_t *scr = take_565(&ver, &r, &any, since < 0 ? 0 : (uint32_t)since, since >= 0);
    if (!scr)
    {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "no memory for a picture now\n");
        return ESP_OK;
    }
    const int W = scr->header.w, H = scr->header.h;
    int x1 = 0, y1 = 0, x2 = -1, y2 = -1;
    if (any)
    {
        x1 = clampi(r.x1, 0, W - 1);
        y1 = clampi(r.y1, 0, H - 1);
        x2 = clampi(r.x2, 0, W - 1);
        y2 = clampi(r.y2, 0, H - 1);
        if (scale == 2)
        {
            x1 &= ~1;
            y1 &= ~1;
            x2 |= 1;
            y2 |= 1;
        }
    }
    const int w = x2 - x1 + 1, h = y2 - y1 + 1;
    static char hver[12], hrect[32], hscale[4];
    snprintf(hver, sizeof hver, "%lu", (unsigned long)ver);
    snprintf(hrect, sizeof hrect, "%d,%d,%d,%d", any ? x1 : 0, any ? y1 : 0, any ? w : 0, any ? h : 0);
    snprintf(hscale, sizeof hscale, "%d", scale);
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Expose-Headers", "X-Frame-Ver, X-Rect, X-Scale, X-Format");
    httpd_resp_set_hdr(req, "X-Format", rle ? "rle565" : "rgb565");
    httpd_resp_set_hdr(req, "X-Frame-Ver", hver);
    httpd_resp_set_hdr(req, "X-Rect", hrect);
    httpd_resp_set_hdr(req, "X-Scale", hscale);
    if (!any)
    {
        httpd_resp_send(req, NULL, 0);
        lv_draw_buf_destroy(scr);
        return ESP_OK;
    }
    /* Sent in blocks of rows: fewer, larger writes go faster over Wi-Fi. */
    const int ow = w / scale, row_bytes = ow * 2;
    const int rows = LV_MAX(1, 16384 / row_bytes);
    uint8_t *out = heap_caps_malloc((size_t)row_bytes * rows, MALLOC_CAP_SPIRAM);
    uint8_t *packed = rle ? heap_caps_malloc((size_t)ow * rows * 3 + 8, MALLOC_CAP_SPIRAM) : NULL;
    esp_err_t err = out && (!rle || packed) ? ESP_OK : ESP_FAIL;
    int filled = 0;
    for (int y = y1; y <= y2 && err == ESP_OK; y += scale)
    {
        const uint16_t *s0 = (const uint16_t *)(scr->data + (size_t)y * scr->header.stride) + x1;
        uint16_t *o = (uint16_t *)(out + (size_t)filled * row_bytes);
        if (scale == 1)
        {
            memcpy(o, s0, row_bytes);
        }
        else
        {
            const uint16_t *s1 = (const uint16_t *)(scr->data + (size_t)(y + 1) * scr->header.stride) + x1;
            for (int x = 0; x < ow; x++)
            {
                const uint16_t a = s0[2 * x], b = s0[2 * x + 1], c = s1[2 * x], d = s1[2 * x + 1];
                const int R = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11) + 2) >> 2;
                const int G = (((a >> 5) & 0x3F) + ((b >> 5) & 0x3F) + ((c >> 5) & 0x3F) + ((d >> 5) & 0x3F) + 2) >> 2;
                const int B = ((a & 0x1F) + (b & 0x1F) + (c & 0x1F) + (d & 0x1F) + 2) >> 2;
                o[x] = (uint16_t)((R << 11) | (G << 5) | B);
            }
        }
        if (++filled == rows || y + scale > y2)
        {
            if (rle)
            {
                const size_t n = rle565((const uint16_t *)out, filled * ow, packed);
                err = httpd_resp_send_chunk(req, (const char *)packed, (ssize_t)n);
            }
            else
            {
                err = httpd_resp_send_chunk(req, (const char *)out, (ssize_t)filled * row_bytes);
            }
            filled = 0;
        }
    }
    if (err == ESP_OK)
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    free(out);
    free(packed);
    lv_draw_buf_destroy(scr);
    return ESP_OK;
}

/* The screen as shown, the top layer over it, as a 24-bit top-down BMP. */
static esp_err_t h_screen(httpd_req_t *req)
{
    if (!allowed(req))
    {
        return ESP_OK;
    }
    lv_draw_buf_t *scr = NULL, *top = NULL;
    PT_LVGL_SCOPE_LOCK()
    {
        scr = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
        if (scr && lv_obj_get_child_count(lv_layer_top()) > 0)
        {
            top = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888);
        }
    }
    const int w = scr ? scr->header.w : 0, h = scr ? scr->header.h : 0;
    const int row = w * 3; /* 800 * 3 is a multiple of 4: no padding */
    uint8_t *line = scr ? heap_caps_malloc(row, MALLOC_CAP_SPIRAM) : NULL;
    if (!line)
    {
        if (scr)
        {
            lv_draw_buf_destroy(scr);
        }
        if (top)
        {
            lv_draw_buf_destroy(top);
        }
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "no memory for a picture now\n");
        return ESP_OK;
    }
    uint8_t hdr[54] = {'B', 'M'};
    const uint32_t size = 54 + (uint32_t)row * h;
    memcpy(hdr + 2, &size, 4);
    uint32_t v = 54;
    memcpy(hdr + 10, &v, 4);
    v = 40;
    memcpy(hdr + 14, &v, 4);
    memcpy(hdr + 18, &w, 4);
    const int32_t nh = -h; /* top-down */
    memcpy(hdr + 22, &nh, 4);
    hdr[26] = 1;
    hdr[28] = 24;
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send_chunk(req, (const char *)hdr, sizeof hdr);
    for (int y = 0; y < h && err == ESP_OK; y++)
    {
        const uint8_t *s = scr->data + (size_t)y * scr->header.stride;
        const uint8_t *t = top && y < (int)top->header.h ? top->data + (size_t)y * top->header.stride : NULL;
        for (int x = 0; x < w; x++)
        {
            uint8_t b = s[x * 3], g = s[x * 3 + 1], r = s[x * 3 + 2];
            if (t && x < (int)top->header.w)
            {
                const uint8_t *p = t + x * 4; /* B G R A */
                const int a = p[3];
                b = (uint8_t)((p[0] * a + b * (255 - a)) / 255);
                g = (uint8_t)((p[1] * a + g * (255 - a)) / 255);
                r = (uint8_t)((p[2] * a + r * (255 - a)) / 255);
            }
            line[x * 3] = b;
            line[x * 3 + 1] = g;
            line[x * 3 + 2] = r;
        }
        err = httpd_resp_send_chunk(req, (const char *)line, row);
    }
    if (err == ESP_OK)
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    free(line);
    lv_draw_buf_destroy(scr);
    if (top)
    {
        lv_draw_buf_destroy(top);
    }
    return ESP_OK;
}

/* ---- The server -------------------------------------------------------------------- */

static void server_start(void)
{
    if (s_srv)
    {
        return;
    }
    ensure_hooks();
    if (!s_lpq)
    {
        s_lpq = xQueueCreate(LP_MAX, sizeof(lp_t));
        if (!s_lpq || xTaskCreatePinnedToCoreWithCaps(lp_task, "lan_wait", 3072, NULL, 4, NULL, 0,
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
        {
            ESP_LOGE(TAG, "no task for held requests: the long poll answers at once");
            if (s_lpq)
            {
                vQueueDelete(s_lpq);
                s_lpq = NULL;
            }
        }
    }
    s_stopping = false;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    /* A screenshot draws the screen on this task (lv_snapshot_take): 5.4 KB at
     * the peak, measured under load; 6 KB overflowed. In PSRAM, so cheap. */
    cfg.stack_size = 16 * 1024;
    cfg.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT; /* the internal RAM is the limit */
    cfg.core_id = 0;                                    /* core 1 is the screen's */
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_srv, &cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "the server did not start");
        s_srv = NULL;
        return;
    }
    const httpd_uri_t uris[] = {
        {.uri = "/screen.raw", .method = HTTP_GET, .handler = h_raw},
        {.uri = "/screen.ver", .method = HTTP_GET, .handler = h_version},
        {.uri = "/screen.bmp", .method = HTTP_GET, .handler = h_screen},
        {.uri = "/api/tap", .method = HTTP_POST, .handler = h_tap},
        {.uri = "/api/swipe", .method = HTTP_POST, .handler = h_swipe},
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++)
    {
        httpd_register_uri_handler(s_srv, &uris[i]);
    }
    ESP_LOGI(TAG, "live view served on port 80");
}

static void server_stop(void)
{
    if (s_srv)
    {
        s_stopping = true; /* the held requests are answered first */
        vTaskDelay(pdMS_TO_TICKS(50));
        httpd_stop(s_srv);
        s_srv = NULL;
        ESP_LOGI(TAG, "live view stopped");
    }
}

void lan_mode_set_live(bool on)
{
    s_live = on;
    settings_set("lan_live", on ? "1" : "0");
    s_version++;
    if (on)
    {
        server_start();
    }
    else
    {
        server_stop();
    }
}

void lan_mode_init(void)
{
    char v[CODE_LEN + 2] = "";
    if (settings_get("lan_code", v, sizeof v) && code_valid(v))
    {
        strlcpy(s_code, v, sizeof s_code);
    }
    else
    {
        draw_code(); /* the first start: kept from then on, across updates */
    }
    char l[4] = "";
    s_live = !settings_get("lan_live", l, sizeof l) || l[0] != '0'; /* on unless switched off */
    if (s_live)
    {
        server_start();
    }
}
