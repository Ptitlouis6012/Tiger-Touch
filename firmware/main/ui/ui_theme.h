#pragma once

#include "lvgl.h"

/* Faces with Latin-1 + Latin Extended-A + LVGL symbols (see fonts/README.md).
 * Use these for anything that carries accents; lv_font_montserrat_* is ASCII
 * only and is kept for digits. */
LV_FONT_DECLARE(font_ui_14);
LV_FONT_DECLARE(font_ui_16);
LV_FONT_DECLARE(font_ui_20);
LV_FONT_DECLARE(font_ui_24);
LV_FONT_DECLARE(font_icon_32); /* FontAwesome's sun alone, 32 px (tools/make_fonts.py) */

/* Palette of the approved mockups. */
#define C_BG       lv_color_hex(0x14161a)
#define C_CARD     lv_color_hex(0x1e2128)
#define C_TILE     lv_color_hex(0x262a33)
#define C_LINE     lv_color_hex(0x2b2f38)
#define C_TEXT     lv_color_hex(0xf2f2f2)
#define C_MUTED    lv_color_hex(0x8b909a)
#define C_ACCENT   lv_color_hex(0xf58220)
#define C_GREEN    lv_color_hex(0x3ecf8e)
#define C_RED      lv_color_hex(0xf06a6a)
#define C_AMBER    lv_color_hex(0xf5b84a) /* paused */
