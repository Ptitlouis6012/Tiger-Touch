#pragma once

#include "lvgl.h"

/* The logo of a printer brand, as a 32x32 alpha mask to paint in one colour
 * (lv_obj_set_style_image_recolor). `brand` is the account's brand id
 * ("bambulab", "creality"...); NULL when there is none. */
const lv_image_dsc_t *brand_logo(const char *brand);
