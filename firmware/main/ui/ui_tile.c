#include "ui_tile.h"

#include "ui_theme.h"

void ui_tile_build(ui_tile_t *t, lv_obj_t *parent, picto_t what, lv_color_t bg, int radius)
{
    t->tile = lv_obj_create(parent);
    lv_obj_set_style_bg_color(t->tile, bg, 0);
    lv_obj_set_style_bg_opa(t->tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t->tile, 0, 0);
    lv_obj_set_style_border_color(t->tile, C_ACCENT, 0);
    lv_obj_set_style_radius(t->tile, radius, 0);
    lv_obj_set_style_pad_all(t->tile, 0, 0);
    lv_obj_set_style_pad_column(t->tile, 8, 0);
    lv_obj_set_scrollable(t->tile, false);
    lv_obj_set_clickable(t->tile, false);
    lv_obj_set_flex_flow(t->tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(t->tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_hidden(t->tile, true);

    t->ico = picto_create(t->tile, what, UI_TILE_ICON, C_MUTED);

    t->col = lv_obj_create(t->tile);
    lv_obj_set_size(t->col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(t->col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(t->col, 0, 0);
    lv_obj_set_style_pad_all(t->col, 0, 0);
    lv_obj_set_style_pad_row(t->col, 0, 0);
    lv_obj_set_scrollable(t->col, false);
    lv_obj_set_clickable(t->col, false);
    lv_obj_set_flex_flow(t->col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(t->col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    t->cap = lv_label_create(t->col);
    lv_label_set_text(t->cap, "");
    lv_obj_set_style_text_font(t->cap, &font_ui_14, 0);
    lv_obj_set_style_text_color(t->cap, C_MUTED, 0);
    lv_obj_set_hidden(t->cap, true);

    t->val = lv_label_create(t->col);
    lv_label_set_text(t->val, "--");
    lv_obj_set_style_text_font(t->val, &font_ui_16, 0);
    lv_obj_set_style_text_color(t->val, C_TEXT, 0);
}

void ui_tile_place(ui_tile_t *t, int x, int y, int w, int h, const lv_font_t *val_font, bool with_cap)
{
    lv_obj_set_pos(t->tile, x, y);
    lv_obj_set_size(t->tile, w, h);
    lv_obj_set_style_text_font(t->val, val_font, 0);
    lv_obj_set_hidden(t->cap, !with_cap);
    lv_obj_set_hidden(t->tile, false);
}
