#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "pictos.h"

/* A temperature tile: the pictogram of what is measured (nozzle, bed, chamber)
 * and its value, centred together in the tile. The printers page and the full
 * view build the same tile, so they read alike.
 *
 * A printer with several nozzles also gets a short caption (Left, Right...)
 * over the value where the tile is wide enough for one: a pictogram cannot say
 * which side a nozzle is on. */
typedef struct
{
    lv_obj_t *tile;
    lv_obj_t *ico;
    lv_obj_t *col; /* the caption over the value, centred as one block */
    lv_obj_t *cap; /* hidden unless the layout asks for it */
    lv_obj_t *val;
} ui_tile_t;

#define UI_TILE_ICON 24

/* Builds a tile under `parent`, hidden. Needs the LVGL lock. */
void ui_tile_build(ui_tile_t *t, lv_obj_t *parent, picto_t what, lv_color_t bg, int radius);

/* Puts the tile at a place and size, shows it, and chooses the value's font and
 * whether the caption is drawn. */
void ui_tile_place(ui_tile_t *t, int x, int y, int w, int h, const lv_font_t *val_font, bool with_cap);
