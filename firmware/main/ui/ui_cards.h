#pragma once

#include "lvgl.h"
#include "ui_dashboard.h"
#include "ui_setup.h"

/* The printers page: one card per switched-on printer, in a row that scrolls
 * sideways, each showing its printer live. Touching a card posts the same event
 * the list did, so the flow that opens the full view is unchanged.
 *
 * Each card also carries the pause / resume and stop buttons (ui_job.h), drawn
 * only while a job runs on a connection that takes commands. */

/* Builds the row on `parent` (a screen) and returns it. `on_pick` is called with
 * the card's index as user data when one is touched. Needs the LVGL lock. */
lv_obj_t *ui_cards_create(lv_obj_t *parent, const printer_row_t *rows, int n, lv_event_cb_t on_pick);

/* Gives card `index` its printer's view, once a second. `chip` says whether the
 * printer answers, is silent, or is still being reached. A card that is not on
 * the screen being shown is left alone. Needs the LVGL lock. */
void ui_cards_update(int index, const printer_view_t *v, printer_chip_t chip);

/* List view only: puts the rows in order of the estimated end of the print, the
 * printer that ends first at the top, those with no print running last. Called
 * after the cards are updated. The rows stay still for 3 s after a touch, so
 * that none moves under a finger. Needs the LVGL lock. */
void ui_cards_sort(void);

/* The home's two views: the cards (the default) or a vertical list of light
 * rows. Chosen before ui_cards_create; the setting is kept by main.c. */
void ui_cards_set_list(bool list);
bool ui_cards_is_list(void);

/* The screen is going away: forget the cards. */
void ui_cards_forget(void);
