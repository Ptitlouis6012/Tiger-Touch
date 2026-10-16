#include "ui_job.h"

#include "pictos.h"
#include "ui_setup.h"
#include "ui_theme.h"

#define HOLD_MS 1500
#define DARK_RED lv_color_hex(0x7a2f2f)
#define LIGHT_ON lv_color_hex(0xf5b84a)

static void on_pause(lv_event_t *e)
{
    job_buttons_t *b = (job_buttons_t *)lv_event_get_user_data(e);
    ui_post(EV_PRINTER_JOB, b->index, b->paused ? "resume" : "pause", NULL);
}

static void on_light(lv_event_t *e)
{
    job_buttons_t *b = (job_buttons_t *)lv_event_get_user_data(e);
    ui_post(EV_PRINTER_JOB, b->index, b->light_on > 0 ? "light_off" : "light_on", NULL);
}

static void fill_cb(void *obj, int32_t w)
{
    lv_obj_set_width((lv_obj_t *)obj, w);
}

static void hold_done(lv_anim_t *a)
{
    job_buttons_t *b = (job_buttons_t *)lv_anim_get_user_data(a);
    lv_obj_set_width(b->fill, 0);
    ui_post(EV_PRINTER_JOB, b->index, "stop", NULL);
}

static void cancel_hold(job_buttons_t *b)
{
    lv_anim_delete(b->fill, fill_cb);
    lv_obj_set_width(b->fill, 0);
}

static void on_stop(lv_event_t *e)
{
    job_buttons_t *b = (job_buttons_t *)lv_event_get_user_data(e);
    switch (lv_event_get_code(e))
    {
    case LV_EVENT_PRESSED:
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, b->fill);
        lv_anim_set_exec_cb(&a, fill_cb);
        lv_anim_set_values(&a, 0, b->stop_w);
        lv_anim_set_duration(&a, HOLD_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_linear);
        lv_anim_set_completed_cb(&a, hold_done);
        lv_anim_set_user_data(&a, b);
        lv_anim_start(&a);
        break;
    }
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        cancel_hold(b);
        break;
    default:
        break;
    }
}

static lv_obj_t *mk_button(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, C_TILE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 10, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, true);
    return o;
}

/* A button this tall (the printers page's and the full view's) takes a larger symbol. */
#define BIG_BUTTON_H 44

static lv_obj_t *mk_icon(lv_obj_t *parent, const char *symbol, lv_color_t col, int button_h)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, button_h >= BIG_BUTTON_H ? &lv_font_montserrat_24 : &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_center(l);
    return l;
}

/* The pair is drawn all the time; "active" makes it clickable and full strength,
 * the rest of the time it is dimmed and takes no touch. */
static void set_active(job_buttons_t *b, bool active)
{
    b->shown = active;
    lv_obj_set_clickable(b->pause, active);
    lv_obj_set_clickable(b->stop, active);
    lv_obj_set_style_opa(b->pause, active ? LV_OPA_COVER : LV_OPA_40, 0);
    lv_obj_set_style_opa(b->stop, active ? LV_OPA_COVER : LV_OPA_40, 0);
}

void ui_job_build(job_buttons_t *b, lv_obj_t *parent, int px, int py, int sx, int sy, int w, int h, int lx, int ly,
                  int lw, int lh, int index)
{
    b->index = index;
    b->stop_w = w;
    b->shown = false;
    b->paused = false;
    b->light_can = -1;
    b->light_on = -1;

    b->light = mk_button(parent, lx, ly, lw, lh);
    lv_obj_set_style_bg_color(b->light, C_LINE, LV_STATE_PRESSED);
    b->light_icon = picto_create(b->light, PICTO_BULB, 24, C_MUTED);
    if (lh >= BIG_BUTTON_H)
    {
        lv_image_set_scale(b->light_icon, 341); /* 24 px mask drawn at 32 */
    }
    lv_obj_center(b->light_icon);
    lv_obj_add_event_cb(b->light, on_light, LV_EVENT_CLICKED, b);
    ui_job_set_light(b, false, false);

    b->pause = mk_button(parent, px, py, w, h);
    lv_obj_set_style_bg_color(b->pause, C_LINE, LV_STATE_PRESSED);
    b->pause_icon = mk_icon(b->pause, LV_SYMBOL_PAUSE, C_TEXT, h);
    lv_obj_add_event_cb(b->pause, on_pause, LV_EVENT_CLICKED, b);

    b->stop = mk_button(parent, sx, sy, w, h);
    lv_obj_set_style_border_width(b->stop, 2, 0);
    lv_obj_set_style_border_color(b->stop, C_RED, 0);
    b->fill = lv_obj_create(b->stop);
    lv_obj_set_pos(b->fill, 0, 0);
    lv_obj_set_size(b->fill, 0, h - 4); /* inside the 2 px frame */
    lv_obj_set_style_bg_color(b->fill, DARK_RED, 0);
    lv_obj_set_style_bg_opa(b->fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b->fill, 0, 0);
    lv_obj_set_style_radius(b->fill, 8, 0);
    lv_obj_set_style_pad_all(b->fill, 0, 0);
    lv_obj_set_scrollable(b->fill, false);
    lv_obj_set_clickable(b->fill, false);
    b->stop_icon = mk_icon(b->stop, LV_SYMBOL_STOP, C_RED, h);
    lv_obj_add_event_cb(b->stop, on_stop, LV_EVENT_ALL, b);

    /* Always drawn, like the light; inert until a job runs on a connection that
     * takes commands (see ui_job_set). */
    set_active(b, false);
}

void ui_job_set_light(job_buttons_t *b, bool can, bool on)
{
    if (!b->light)
    {
        return;
    }
    if ((int)can != b->light_can)
    {
        b->light_can = can;
        lv_obj_set_clickable(b->light, can);
        lv_obj_set_style_opa(b->light, can ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    if ((int)on != b->light_on)
    {
        b->light_on = on;
        lv_obj_set_style_image_recolor(b->light_icon, on ? LIGHT_ON : C_MUTED, 0);
    }
}

void ui_job_set(job_buttons_t *b, bool active, bool paused)
{
    if (!b->pause)
    {
        return;
    }
    if (active != b->shown)
    {
        set_active(b, active);
        if (!active)
        {
            cancel_hold(b); /* a hold must not complete on a button that went inert */
        }
    }
    if (paused != b->paused)
    {
        b->paused = paused;
        lv_label_set_text(b->pause_icon, paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
    }
}
