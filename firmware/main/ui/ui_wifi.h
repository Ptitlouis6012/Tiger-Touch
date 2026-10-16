#pragma once

#include <stdbool.h>

#include "cloud.h"
#include "lvgl.h"

/* The Wi-Fi wave, ported from the TigerSpool so that the same signal reads the
 * same on every Tiger device: the glyph of the UI font drawn dimmed, and a
 * second, clipped copy lit up to the level. Level 0..3.
 *
 * The scale sits 10 dB below a textbook's on purpose: the ESP32's receiver
 * reads low, by an amount that is not even constant between boards, and a scale
 * built for a phone's numbers would tell a user their network is failing while
 * it works. The level is smoothed and has to be earned (3 dB past a boundary to
 * gain an arc, 3 dB back to lose one), so the icon does not twitch. */

lv_obj_t *ui_wifi_wave(lv_obj_t *parent);
void ui_wifi_wave_set(lv_obj_t *box, int level, bool connected);

/* No memory: what a list of networks shows for each one. */
int ui_wifi_level_from_rssi(int rssi);

/* The waves kept up to date by one LVGL timer: the persistent one (the
 * dashboard's, never deleted) and a transient one (the list's, forgotten when
 * its screen goes). */
void ui_wifi_set_persistent(lv_obj_t *box);
void ui_wifi_set_transient(lv_obj_t *box);
void ui_wifi_refresh_now(void);

/* The dashboard is about to be deleted and built again (a language change):
 * forget its wave and its account icon WITHOUT looking at them. They point at
 * objects that are already gone, and asking LVGL whether such a pointer is
 * valid reads freed memory. The new ones are registered as they are built. */
void ui_wifi_forget_persistent(void);

/* The account, drawn like the TigerScale's: a head over shoulders cut into a
 * dome, no ring around it. Green when the Firebase account answers, orange
 * while a stored account is being reached, red when there is none. It sits
 * beside the wave and rides the same timer: set it with the same lifetime as
 * the wave it accompanies (the transient one is forgotten with the wave). */
lv_obj_t *ui_account_icon(lv_obj_t *parent);
void ui_account_icon_set(lv_obj_t *box, cloud_link_t link);
void ui_account_set_persistent(lv_obj_t *box);
void ui_account_set_transient(lv_obj_t *box);
