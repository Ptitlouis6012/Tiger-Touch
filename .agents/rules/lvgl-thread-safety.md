# LVGL from one place at a time

**Rule.** Anything that touches a widget holds the LVGL lock (`PT_LVGL_SCOPE_LOCK`), and a pointer to a widget kept by a timer is forgotten before the widget is deleted, without being tested.

**Why.** Founder rule. LVGL is not re-entrant; asking LVGL whether a freed object is valid reads freed memory.

**How.**
- See `ui_wifi_forget_persistent` for the pattern.
- `CODEMAP.md` lists the landmines of each module.
