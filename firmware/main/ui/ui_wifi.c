#include "ui_wifi.h"

#include "ui_theme.h"
#include "wifi_mgr.h"

/* Geometry read from font_ui_24's glyph, as the TigerSpool did: the box is the
 * glyph's own, and the clip heights are cut in the gaps of its bitmap, so a
 * level never crosses ink. */
#define WIFI_W 30
#define WIFI_H 23
#define WIFI_DROP 5
#define WIFI_CLIP_W 22
static const uint8_t CLIP_H[4] = {0, 7, 14, WIFI_H};

/* ---- The scale ------------------------------------------------------------ */

#define MARGIN_DB 3
#define SAMPLE_MS 1000
#define JUMP_DB 15

int ui_wifi_level_from_rssi(int rssi)
{
    if (rssi >= -70)
    {
        return 3;
    }
    if (rssi >= -80)
    {
        return 2;
    }
    if (rssi >= -90)
    {
        return 1;
    }
    return 0;
}

static int floor_for(int level)
{
    switch (level)
    {
    case 3:  return -70;
    case 2:  return -80;
    case 1:  return -90;
    default: return -1000;
    }
}

typedef struct
{
    float avg;        /* 0 = nothing seen yet */
    int level;        /* -1 = not decided yet */
    uint32_t last_at;
} smoother_t;

static smoother_t s_smooth = {0.0f, -1, 0};

static int smooth_update(int rssi, uint32_t now_ms)
{
    smoother_t *s = &s_smooth;
    if (rssi == 0)
    {
        s->avg = 0.0f;
        s->level = -1;
        s->last_at = 0;
        return 0;
    }
    const bool jumped = s->avg != 0.0f && (rssi - s->avg > JUMP_DB || s->avg - rssi > JUMP_DB);
    if (s->avg != 0.0f && !jumped && s->last_at && now_ms - s->last_at < SAMPLE_MS)
    {
        return s->level < 0 ? ui_wifi_level_from_rssi((int)s->avg) : s->level;
    }
    s->last_at = now_ms;
    if (s->avg == 0.0f || jumped)
    {
        s->avg = (float)rssi;
    }
    else
    {
        s->avg = s->avg * 0.8f + (float)rssi * 0.2f;
    }
    if (s->level < 0)
    {
        s->level = ui_wifi_level_from_rssi((int)s->avg);
        return s->level;
    }
    if (s->level < 3 && s->avg >= (float)(floor_for(s->level + 1) + MARGIN_DB))
    {
        s->level++;
    }
    else if (s->level > 0 && s->avg <= (float)(floor_for(s->level) - MARGIN_DB))
    {
        s->level--;
    }
    return s->level;
}

/* ---- The widget ----------------------------------------------------------- */

lv_obj_t *ui_wifi_wave(lv_obj_t *parent)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_size(wrap, WIFI_W, WIFI_H);
    lv_obj_set_scrollable(wrap, false);
    lv_obj_set_clickable(wrap, false);

    lv_obj_t *dim = lv_label_create(wrap); /* child 0 */
    lv_label_set_text(dim, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(dim, &font_ui_24, 0);
    lv_obj_align(dim, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);

    lv_obj_t *clip = lv_obj_create(wrap); /* child 1 */
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, WIFI_CLIP_W, WIFI_H);
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_scrollable(clip, false);
    lv_obj_set_clickable(clip, false);

    lv_obj_t *lit = lv_label_create(clip);
    lv_label_set_text(lit, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lit, &font_ui_24, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);

    ui_wifi_wave_set(wrap, 0, false);
    return wrap;
}

void ui_wifi_wave_set(lv_obj_t *box, int level, bool connected)
{
    if (!box || lv_obj_get_child_count(box) < 2)
    {
        return;
    }
    lv_obj_t *dim = lv_obj_get_child(box, 0);
    lv_obj_t *clip = lv_obj_get_child(box, 1);
    lv_obj_t *lit = lv_obj_get_child(clip, 0);
    if (!lit)
    {
        return;
    }

    if (!connected)
    {
        lv_obj_set_style_text_color(dim, C_RED, 0);
        lv_obj_set_style_text_opa(dim, LV_OPA_COVER, 0);
        lv_obj_set_hidden(clip, true);
        return;
    }
    if (level < 0)
    {
        level = 0;
    }
    if (level > 3)
    {
        level = 3;
    }

    /* Full signal is one uniform glyph: a clip can only cover the ink it
     * reaches, and nothing is left to dim at full strength. */
    if (level >= 3)
    {
        lv_obj_set_style_text_color(dim, C_GREEN, 0);
        lv_obj_set_style_text_opa(dim, LV_OPA_COVER, 0);
        lv_obj_set_hidden(clip, true);
        return;
    }

    lv_obj_set_style_text_color(dim, C_GREEN, 0);
    lv_obj_set_style_text_opa(dim, LV_OPA_50, 0);
    lv_obj_set_style_text_color(lit, C_GREEN, 0);
    lv_obj_set_style_text_opa(lit, LV_OPA_COVER, 0);

    if (level == 0)
    {
        lv_obj_set_hidden(clip, true);
        return;
    }
    lv_obj_set_hidden(clip, false);
    lv_obj_set_height(clip, CLIP_H[level]);
    /* LVGL does not re-run alignment after a size change: without these two the
     * clip re-centres and the lit copy drifts off the dimmed one. */
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);
}

/* One step of "connecting": the arcs light up from the bottom one after the
 * other, as macOS does, in the text colour over a faint glyph - something is
 * happening, nothing is wrong. Red is kept for a failure. Steps 1, 2, 3 light
 * one, two, three levels; step 0 is the faint glyph alone, before it starts
 * again. */
static void wave_connecting(lv_obj_t *box, int step)
{
    if (!box || lv_obj_get_child_count(box) < 2)
    {
        return;
    }
    lv_obj_t *dim = lv_obj_get_child(box, 0);
    lv_obj_t *clip = lv_obj_get_child(box, 1);
    lv_obj_t *lit = lv_obj_get_child(clip, 0);
    if (!lit)
    {
        return;
    }
    lv_obj_set_style_text_color(dim, C_TEXT, 0);
    lv_obj_set_style_text_opa(dim, step >= 3 ? LV_OPA_COVER : LV_OPA_30, 0);
    lv_obj_set_style_text_color(lit, C_TEXT, 0);
    lv_obj_set_style_text_opa(lit, LV_OPA_COVER, 0);
    if (step <= 0 || step >= 3)
    {
        lv_obj_set_hidden(clip, true);
        return;
    }
    lv_obj_set_hidden(clip, false);
    lv_obj_set_height(clip, CLIP_H[step]);
    lv_obj_align(clip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_align(lit, LV_ALIGN_BOTTOM_MID, 0, WIFI_DROP);
}

/* ---- The account ---------------------------------------------------------- */

/* The TigerScale's 16 px silhouette (head 7, shoulders 14 clipped to a dome)
 * scaled by 1.5 to the wave's 24 px glyph. The shoulders disc is wider than
 * the box on purpose: the parent's clip is what makes it a pair of shoulders. */
#define ACCT_SIZE 24

lv_obj_t *ui_account_icon(lv_obj_t *parent)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_size(wrap, ACCT_SIZE, ACCT_SIZE);
    lv_obj_set_scrollable(wrap, false);
    lv_obj_set_clickable(wrap, false);

    lv_obj_t *head = lv_obj_create(wrap); /* child 0 */
    lv_obj_remove_style_all(head);
    lv_obj_set_size(head, 10, 10);
    lv_obj_set_pos(head, 7, 1);
    lv_obj_set_style_radius(head, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(head, LV_OPA_COVER, 0);
    lv_obj_set_clickable(head, false);

    lv_obj_t *shoulders = lv_obj_create(wrap); /* child 1 */
    lv_obj_remove_style_all(shoulders);
    lv_obj_set_size(shoulders, 22, 22);
    lv_obj_set_pos(shoulders, 1, 14);
    lv_obj_set_style_radius(shoulders, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(shoulders, LV_OPA_COVER, 0);
    lv_obj_set_clickable(shoulders, false);

    ui_account_icon_set(wrap, CLOUD_LINK_NONE);
    return wrap;
}

void ui_account_icon_set(lv_obj_t *box, cloud_link_t link)
{
    if (!box || lv_obj_get_child_count(box) < 2)
    {
        return;
    }
    const lv_color_t col = link == CLOUD_LINK_OK ? C_GREEN : (link == CLOUD_LINK_CONNECTING ? C_ACCENT : C_RED);
    lv_obj_set_style_bg_color(lv_obj_get_child(box, 0), col, 0);
    lv_obj_set_style_bg_color(lv_obj_get_child(box, 1), col, 0);
}

/* ---- Keeping them current ------------------------------------------------- */

static lv_obj_t *s_persistent;
static lv_obj_t *s_transient;
static lv_obj_t *s_acct_persistent;
static lv_obj_t *s_acct_transient;
static lv_timer_t *s_timer;
static int s_step; /* the step of the "connecting" animation */

#define TICK_MS 1000      /* the signal, once a second */
#define CONNECTING_MS 300 /* one level lit after the other while connecting */

void ui_wifi_refresh_now(void)
{
    const wifi_state_t state = wifi_mgr_state();
    const int rssi = wifi_mgr_rssi();
    const bool connected = state == WIFI_STATE_CONNECTED && rssi != 0;
    const int level = smooth_update(rssi, lv_tick_get());
    const bool connecting = state == WIFI_STATE_CONNECTING;
    if (s_timer)
    {
        lv_timer_set_period(s_timer, connecting ? CONNECTING_MS : TICK_MS);
    }
    s_step = connecting ? (s_step + 1) % 4 : 0;
    /* A screen can go without telling us: never touch a widget that is gone. */
    if (s_persistent && !lv_obj_is_valid(s_persistent))
    {
        s_persistent = NULL;
    }
    if (s_transient && !lv_obj_is_valid(s_transient))
    {
        s_transient = NULL;
    }
    if (s_persistent)
    {
        connecting ? wave_connecting(s_persistent, s_step) : ui_wifi_wave_set(s_persistent, level, connected);
    }
    if (s_transient)
    {
        connecting ? wave_connecting(s_transient, s_step) : ui_wifi_wave_set(s_transient, level, connected);
    }

    if (s_acct_persistent && !lv_obj_is_valid(s_acct_persistent))
    {
        s_acct_persistent = NULL;
    }
    if (s_acct_transient && !lv_obj_is_valid(s_acct_transient))
    {
        s_acct_transient = NULL;
    }
    if (s_acct_persistent || s_acct_transient)
    {
        const cloud_link_t link = cloud_link();
        ui_account_icon_set(s_acct_persistent, link);
        ui_account_icon_set(s_acct_transient, link);
    }
}

static void on_timer(lv_timer_t *t)
{
    (void)t;
    ui_wifi_refresh_now();
}

static void ensure_timer(void)
{
    if (!s_timer)
    {
        s_timer = lv_timer_create(on_timer, TICK_MS, NULL);
    }
}

void ui_wifi_set_persistent(lv_obj_t *box)
{
    s_persistent = box;
    ensure_timer();
    ui_wifi_refresh_now();
}

void ui_wifi_set_transient(lv_obj_t *box)
{
    s_transient = box;
    if (box)
    {
        ensure_timer();
        ui_wifi_refresh_now();
    }
    else
    {
        /* The page is going: its account icon goes with it. */
        s_acct_transient = NULL;
    }
}

void ui_wifi_forget_persistent(void)
{
    s_persistent = NULL;
    s_acct_persistent = NULL;
}

void ui_account_set_persistent(lv_obj_t *box)
{
    s_acct_persistent = box;
    ensure_timer();
    ui_wifi_refresh_now();
}

void ui_account_set_transient(lv_obj_t *box)
{
    s_acct_transient = box;
    if (box)
    {
        ensure_timer();
        ui_wifi_refresh_now();
    }
}
