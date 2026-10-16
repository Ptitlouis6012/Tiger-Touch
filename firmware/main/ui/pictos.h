#pragma once

#include <stddef.h>

#include "lvgl.h"

typedef enum
{
    PICTO_NOZZLE,
    PICTO_BED,
    PICTO_CHAMBER,
    PICTO_FILE,
    PICTO_CLOCK,
    PICTO_FLAG,
    PICTO_BULB,
} picto_t;

/* A pictogram as a square alpha mask to paint in one colour
 * (lv_obj_set_style_image_recolor). The temperatures and the bulb exist at 24 px,
 * the labels (file, clock, flag) at 16; NULL for a size that was not made. */
const lv_image_dsc_t *picto(picto_t id, int size);

/* A mask of `size` in `parent`, painted `color`; the image is not clickable. */
lv_obj_t *picto_create(lv_obj_t *parent, picto_t id, int size, lv_color_t color);
