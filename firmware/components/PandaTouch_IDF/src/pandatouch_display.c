#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#include "sdkconfig.h"
#include "pandatouch_display.h"
#include "pandatouch_lvgl_touch.h"
#include "pandatouch_board.h"

#ifdef CONFIG_LV_USE_CUSTOM_MALLOC
#ifdef CONFIG_PT_LVGL_USE_PT_INTERNAL_MALLOC
void lv_mem_init(void)
{
}
void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}
#endif
#endif

/* ====================== Logging / Globals ====================== */
static const char *TAG = "PandaTouch::Display";
static esp_lcd_panel_handle_t pt_lcd_panel_handle = NULL;
static esp_timer_handle_t pt_lvgl_tick = NULL;
static SemaphoreHandle_t pt_lvgl_mutex = NULL;
static volatile uint32_t pt_backlight_setting = PT_BL_MAX;
static lv_display_t *pt_disp = NULL;
TaskHandle_t pt_task_handle_lvgl = NULL;

/* ====================== LVGL mutex ====================== */
static void pt_display_ensure_lvgl_mutex(void)
{
    if (pt_lvgl_mutex == NULL)
    {
        pt_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(pt_lvgl_mutex && "Failed to create LVGL mutex");
    }
}

void pt_lvgl_lock(void)
{
    if (pt_lvgl_mutex)
        xSemaphoreTakeRecursive(pt_lvgl_mutex, portMAX_DELAY);
}

void pt_lvgl_unlock(void)
{
    if (pt_lvgl_mutex)
        xSemaphoreGiveRecursive(pt_lvgl_mutex);
}

/* ====================== Backlight helpers ====================== */

static uint32_t pt_backlight_percent_to_duty(uint32_t percent)
{
    if (percent > PT_BL_MAX)
        percent = PT_BL_MAX;
    uint32_t maxd = (1u << PT_BL_LEDC_RESOLUTION) - 1u;
    return (uint32_t)((percent / 100.0f) * maxd);
}

static void pt_backlight_init(int duty_percent)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PT_BL_PIN) | (1ULL << PT_LCD_RESET_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io);
    gpio_set_level(PT_LCD_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PT_LCD_RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    ledc_timer_config_t tcfg = {
        .speed_mode = PT_BL_LEDC_SPEED_MODE,
        .duty_resolution = PT_BL_LEDC_RESOLUTION,
        .timer_num = PT_BL_LEDC_TIMER,
        .freq_hz = PT_BL_FREQUENCY_HZ,
        .clk_cfg = LEDC_USE_APB_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t ccfg = {
        .gpio_num = PT_BL_PIN,
        .speed_mode = PT_BL_LEDC_SPEED_MODE,
        .channel = PT_BL_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PT_BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
    ledc_fade_func_install(0);

    if (duty_percent < PT_BL_MIN)
        duty_percent = PT_BL_MIN;
    if (duty_percent > PT_BL_MAX)
        duty_percent = PT_BL_MAX;

    ESP_ERROR_CHECK(ledc_set_duty(PT_BL_LEDC_SPEED_MODE, PT_BL_LEDC_CHANNEL, pt_backlight_percent_to_duty(duty_percent)));
    ESP_ERROR_CHECK(ledc_update_duty(PT_BL_LEDC_SPEED_MODE, PT_BL_LEDC_CHANNEL));
}

bool pt_backlight_set(uint32_t percent)
{
    if (percent > PT_BL_MAX)
    {
        percent = PT_BL_MAX;
    }

    /* update in-memory setting; persistence is not handled here */
    pt_backlight_setting = percent;

    esp_err_t e = ledc_set_duty(PT_BL_LEDC_SPEED_MODE, PT_BL_LEDC_CHANNEL, pt_backlight_percent_to_duty(percent));
    if (e == ESP_OK)
    {
        e = ledc_update_duty(PT_BL_LEDC_SPEED_MODE, PT_BL_LEDC_CHANNEL);
    }

    return (e == ESP_OK) ? true : false;
}

/* ====================== Panel init ====================== */
static esp_err_t pt_lcd_panel_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = PT_LCD_PCLK_HZ,
            .h_res = PT_LCD_H_RES,
            .v_res = PT_LCD_V_RES,
            .hsync_pulse_width = PT_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = PT_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = PT_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = PT_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = PT_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = PT_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = true,
                .hsync_idle_low = false,
                .vsync_idle_low = false,
                .de_idle_high = false},
        },
        .data_width = 16,
        .num_fbs = 2, /* two frame buffers, in PSRAM: upright, LVGL draws the hidden one */
        .bounce_buffer_size_px = CONFIG_PT_LVGL_RENDER_BOUNCING_BUFFER_LINES * PT_LCD_H_RES,
        .psram_trans_align = 64,
        .hsync_gpio_num = PT_LCD_HSYNC_PIN,
        .vsync_gpio_num = PT_LCD_VSYNC_PIN,
        .de_gpio_num = PT_LCD_DE_PIN,
        .pclk_gpio_num = PT_LCD_PCLK_PIN,
        .disp_gpio_num = -1,
        .data_gpio_nums = {PT_LCD_DATA0_PIN, PT_LCD_DATA1_PIN, PT_LCD_DATA2_PIN, PT_LCD_DATA3_PIN, PT_LCD_DATA4_PIN, PT_LCD_DATA5_PIN, PT_LCD_DATA6_PIN, PT_LCD_DATA7_PIN, PT_LCD_DATA8_PIN, PT_LCD_DATA9_PIN, PT_LCD_DATA10_PIN, PT_LCD_DATA11_PIN, PT_LCD_DATA12_PIN, PT_LCD_DATA13_PIN, PT_LCD_DATA14_PIN, PT_LCD_DATA15_PIN},
        .flags = {.fb_in_psram = true},
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &pt_lcd_panel_handle), TAG, "esp_lcd_new_rgb_panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(pt_lcd_panel_handle), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(pt_lcd_panel_handle), TAG, "panel_init");
    return ESP_OK;
}

/* ====================== Direct rendering (Tiger Touch) ======================
 *
 * Upright, LVGL draws into the frame buffer that is not on show, and the
 * panel switches to it at its next refresh: no partial buffer, no copy of the
 * picture, and never a picture half drawn on screen.
 *
 * Measured on the board (2026-10-16), a frame of a scroll (home screen's
 * cards / a list): 155 / ~65 ms with 16-line partial buffers; 55 / 41 ms
 * drawing into the one frame buffer on show - but a scroll then flashed, the
 * panel showing pictures half redrawn; 85 / 60 ms redrawing the whole frame
 * into the hidden buffer each time; 75 / 50 ms here, where only what changed
 * is drawn (LV_DISPLAY_RENDER_MODE_DIRECT: LVGL copies it into the other
 * buffer to keep the two in step; 68-83 / 52 ms measured).
 *
 * Turned half a turn, the picture is turned while it is copied, which needs
 * the partial buffers: that orientation keeps them. */
static bool pt_direct;
static SemaphoreHandle_t pt_vsync;

bool pt_display_direct(void)
{
    return pt_direct;
}

/* Not the vsync: with bounce buffers the panel does not read the frame buffer as
 * it is shown but copies it ahead into the bounce buffers, and takes the buffer
 * last given when that copy starts a new frame, some lines before the vsync. A
 * swap given in that gap is only taken one frame later, while the vsync says
 * "done" now: LVGL then drew into the buffer still being copied, and the panel
 * showed a picture half redrawn for one frame (the flashes seen now and then
 * while scrolling). The driver says when a buffer is really left. */
static bool IRAM_ATTR pt_on_fb_left(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata,
                                    void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(pt_vsync, &woken);
    return woken == pdTRUE;
}

static void pt_flush_full(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    if (!lv_display_flush_is_last(disp))
    {
        lv_display_flush_ready(disp); /* more areas of this frame to come */
        return;
    }
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
#if CONFIG_PT_LVGL_RENDER_BOUNCING_BUFFER_LINES == 0
    /* No bounce buffers: the DMA reads the frame buffer itself, behind the
     * CPU's cache. With them, the copy goes through the cache and sees it all. */
    esp_cache_msync(px_map, (size_t)PT_LCD_H_RES * PT_LCD_V_RES * 2,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
#endif
    xSemaphoreTake(pt_vsync, 0);
    /* Given one of its own frame buffers, the panel switches to it: no copy. */
    esp_lcd_panel_draw_bitmap(panel, 0, 0, PT_LCD_H_RES, PT_LCD_V_RES, px_map);
    /* The next picture goes into the buffer just left: only once the panel has
     * really left it (pt_on_fb_left). */
    xSemaphoreTake(pt_vsync, pdMS_TO_TICKS(50));
    lv_display_flush_ready(disp);
}

/* ====================== LVGL flush & tick ====================== */
/* The panel can be mounted upside down: the picture is then turned half a turn
 * as it is flushed (the pixels of the block in reverse order, the block at the
 * mirrored place) and the touch is mirrored the same way. */
static volatile bool pt_rotate180 = false;

void pt_display_set_rotation180(bool on)
{
    pt_rotate180 = on;
}

bool pt_display_rotation180(void)
{
    return pt_rotate180;
}

static void pt_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    if (panel)
    {
        int x1 = area->x1, y1 = area->y1, x2 = area->x2, y2 = area->y2;
        if (pt_rotate180)
        {
            const int w = (int)lv_display_get_horizontal_resolution(disp);
            const int h = (int)lv_display_get_vertical_resolution(disp);
            const int bpp = (int)lv_color_format_get_size(lv_display_get_color_format(disp));
            const size_t n = (size_t)(area->x2 - area->x1 + 1) * (size_t)(area->y2 - area->y1 + 1);
            for (size_t i = 0, j = n - 1; i < j; i++, j--)
            {
                for (int b = 0; b < bpp; b++)
                {
                    uint8_t t = px_map[i * bpp + b];
                    px_map[i * bpp + b] = px_map[j * bpp + b];
                    px_map[j * bpp + b] = t;
                }
            }
            x1 = w - 1 - area->x2;
            x2 = w - 1 - area->x1;
            y1 = h - 1 - area->y2;
            y2 = h - 1 - area->y1;
        }
        /* esp_lcd x2/y2 are exclusive -> +1 */
        esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, px_map);
    }
    lv_display_flush_ready(disp);
}
static void pt_lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

/* ====================== Buffers ====================== */
static void *pt_malloc_caps(size_t sz, uint32_t caps_primary, uint32_t caps_fallback)
{
    void *p = heap_caps_malloc(sz, caps_primary);
    if (!p && caps_fallback)
        p = heap_caps_malloc(sz, caps_fallback);
    return p;
}

static bool pt_lvgl_setup_buffers(lv_display_t *disp, int hor_res, int ver_res, PT_LVGL_render_method_t method)
{
    const size_t px_size = sizeof(lv_color_t);
    const size_t full_bytes = (size_t)hor_res * (size_t)ver_res * px_size;

    switch (method)
    {
    case PT_LVGL_RENDER_FULL_1:
    {
        lv_color_t *fb1 = (lv_color_t *)pt_malloc_caps(full_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_8BIT);
        if (!fb1)
            return false;
        lv_display_set_buffers(disp, fb1, NULL, full_bytes, LV_DISPLAY_RENDER_MODE_FULL);
        ESP_LOGI(TAG, "Buffers: FULL_1 (1x %u KB PSRAM)", (unsigned)(full_bytes / 1024));
        break;
    }
    case PT_LVGL_RENDER_FULL_2:
    {
        lv_color_t *fb1 = (lv_color_t *)pt_malloc_caps(full_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_8BIT);
        lv_color_t *fb2 = (lv_color_t *)pt_malloc_caps(full_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_8BIT);
        if (!fb1 || !fb2)
        {
            if (fb1)
                heap_caps_free(fb1);
            if (fb2)
                heap_caps_free(fb2);
            return false;
        }
        lv_display_set_buffers(disp, fb1, fb2, full_bytes, LV_DISPLAY_RENDER_MODE_FULL);
        ESP_LOGI(TAG, "Buffers: FULL_2 (2x %u KB PSRAM)", (unsigned)(full_bytes / 1024));
        break;
    }
    case PT_LVGL_RENDER_PARTIAL_1:
    case PT_LVGL_RENDER_PARTIAL_2:
    case PT_LVGL_RENDER_PARTIAL_1_PSRAM:
    case PT_LVGL_RENDER_PARTIAL_2_PSRAM:
    {
        const int lines = CONFIG_PT_LVGL_RENDER_PARTIAL_BUFFER_LINES;
        const size_t part_bytes = (size_t)hor_res * (size_t)lines * px_size;
        const bool psram_first = (method == PT_LVGL_RENDER_PARTIAL_1_PSRAM || method == PT_LVGL_RENDER_PARTIAL_2_PSRAM);
        const uint32_t caps_int = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
        const uint32_t caps_psr = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

        uint32_t primary = psram_first ? caps_psr : caps_int;
        uint32_t fallback = psram_first ? caps_int : caps_psr;

        lv_color_t *pb1 = (lv_color_t *)pt_malloc_caps(part_bytes, primary, fallback);
        if (!pb1)
            return false;

        const bool pingpong = (method == PT_LVGL_RENDER_PARTIAL_2 || method == PT_LVGL_RENDER_PARTIAL_2_PSRAM);
        if (pingpong)
        {
            lv_color_t *pb2 = (lv_color_t *)pt_malloc_caps(part_bytes, primary, fallback);
            if (!pb2)
            {
                heap_caps_free(pb1);
                return false;
            }
            lv_display_set_buffers(disp, pb1, pb2, part_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
        }
        else
        {
            lv_display_set_buffers(disp, pb1, NULL, part_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
        }

        ESP_LOGI(TAG, "Buffers: %s (line %u KB, %d lines, %s-first)",
                 (pingpong ? "PARTIAL_2" : "PARTIAL_1"),
                 (unsigned)(part_bytes / 1024), lines,
                 psram_first ? "PSRAM" : "INTERNAL");
        break;
    }
    default:
        return false;
    }
    return true;
}

/* ====================== LVGL display init ====================== */
static esp_err_t pt_lvgl_display_init(lv_display_t **out_disp,
                                      PT_LVGL_render_method_t method,
                                      lv_color_format_t color_fmt,
                                      lv_display_flush_cb_t flush_cb,
                                      void *user_data)
{
    if (!out_disp)
        return ESP_ERR_INVALID_ARG;

    pt_display_ensure_lvgl_mutex();

    const int32_t hor_res = PT_LCD_H_RES;
    const int32_t ver_res = PT_LCD_V_RES;

    lv_display_t *disp = lv_display_create(hor_res, ver_res);
    if (!disp)
        return ESP_FAIL;

    lv_display_set_color_format(disp, color_fmt);
    lv_display_set_flush_cb(disp, flush_cb ? flush_cb : pt_lvgl_flush_cb);
    lv_display_set_user_data(disp, user_data ? user_data : pt_lcd_panel_handle);

    void *fb0 = NULL, *fb1 = NULL;
    if (!pt_rotate180 && esp_lcd_rgb_panel_get_frame_buffer(pt_lcd_panel_handle, 2, &fb0, &fb1) == ESP_OK && fb0 &&
        fb1 && (pt_vsync = xSemaphoreCreateBinary()) != NULL)
    {
        const esp_lcd_rgb_panel_event_callbacks_t cbs = {.on_frame_buf_complete = pt_on_fb_left};
        esp_lcd_rgb_panel_register_event_callbacks(pt_lcd_panel_handle, &cbs, NULL);
        lv_display_set_flush_cb(disp, pt_flush_full);
        lv_display_set_buffers(disp, fb0, fb1, (size_t)hor_res * (size_t)ver_res * sizeof(lv_color16_t),
                               LV_DISPLAY_RENDER_MODE_DIRECT);
        pt_direct = true;
        ESP_LOGI(TAG, "Buffers: DIRECT into the panel's two frame buffers, swapped when the panel leaves one");
    }
    else if (!pt_lvgl_setup_buffers(disp, hor_res, ver_res, method))
    {
        ESP_LOGE(TAG, "LVGL buffer setup failed");
        return ESP_ERR_NO_MEM;
    }

    *out_disp = disp;
    return ESP_OK;
}

/* ====================== LVGL runtime (tick + task) ====================== */
static void pt_lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");
    uint32_t period = pdMS_TO_TICKS(0);
    while (true)
    {
        PT_LVGL_SCOPE_LOCK()
        {
            period = lv_timer_handler();
        }
        vTaskDelay(period);
    }
}

static esp_err_t pt_lvgl_start_runtime(void)
{
    pt_display_ensure_lvgl_mutex();

    if (pt_lvgl_tick == NULL)
    {
        const esp_timer_create_args_t tick_args = {
            .callback = &pt_lvgl_tick_cb,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "lvgl_tick"};
        ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &pt_lvgl_tick), TAG, "timer_create");
        ESP_RETURN_ON_ERROR(esp_timer_start_periodic(pt_lvgl_tick, 2000), TAG, "timer_start");
    }
    xTaskCreatePinnedToCoreWithCaps(pt_lvgl_task, "lvgl", CONFIG_PT_LVGL_TASK_STACK_SIZE * 1024, NULL, 5, &pt_task_handle_lvgl, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ESP_OK;
}

/* ====================== Public API ====================== */

esp_err_t pt_display_init(void)
{
    /* Backlight early for smooth fade-in */
    pt_backlight_init(5);
    pt_backlight_set(100);

    /* Step 1: LCD panel (no LVGL yet) */
    ESP_RETURN_ON_ERROR(pt_lcd_panel_init(), TAG, "panel_init");

    /* Step 2: LVGL core + display */
    lv_init();
    PT_LVGL_render_method_t method = (PT_LVGL_render_method_t)CONFIG_PT_LVGL_RENDER_METHOD;

    ESP_RETURN_ON_ERROR(pt_lvgl_display_init(&pt_disp,
                                             method,
                                             LV_COLOR_FORMAT_RGB565,
                                             pt_lvgl_flush_cb,
                                             pt_lcd_panel_handle),
                        TAG, "pt_lvgl_display_init");

    lv_indev_t *indev = pt_lvgl_touch_init(pt_disp, 800, 480);
    (void)indev;

    /* Step 3: LVGL runtime (tick + task) */
    ESP_RETURN_ON_ERROR(pt_lvgl_start_runtime(), TAG, "pt_lvgl_start_runtime");

    return ESP_OK;
}

uint32_t pt_backlight_get(void)
{
    return pt_backlight_setting;
}

void pt_display_schedule_ui(pt_ui_fn_t fn, void *arg)
{
    if (!fn)
        return;
    /* Run on LVGL thread */
    lv_async_call(fn, arg);
}

lv_display_t *pt_get_display(void) { return pt_disp; }
esp_lcd_panel_handle_t pt_get_panel(void) { return pt_lcd_panel_handle; }
