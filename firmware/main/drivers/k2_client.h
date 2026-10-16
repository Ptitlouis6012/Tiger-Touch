#pragma once

#include <stdbool.h>

#include "ui_dashboard.h"

/* Live state of one Creality printer (K2 family, Ender-3 V4, K1...), read over
 * its local WebSocket on :9999.
 *
 * What the printer does, measured on a K2:
 *  - no authentication, and no request is needed: it pushes a complete
 *    snapshot (about 80 fields) as soon as the socket opens, then only the
 *    fields that changed (temperatures every ~3 s);
 *  - it sends {"ModeCode":"heart_beat"} and expects the plain text "ok" back;
 *  - the filament boxes are not part of the snapshot: `boxsInfo` is asked for;
 *  - numbers often arrive as strings ("22.930000"). */

typedef struct k2_client k2_client_t;

/* Starts connecting to ws://<ip>:9999 and keeps reconnecting on its own. */
k2_client_t *k2_client_start(const char *ip);
void k2_client_stop(k2_client_t *c);

/* Fills `v` from the latest state (name is left untouched). `v->online` says
 * whether the printer is talking. Cheap enough to call every second. */
void k2_client_snapshot(k2_client_t *c, printer_view_t *v);

/* Pause, resume or stop the print: {"method":"set","params":{"pause":1|0}} and
 * {"stop":1}, the frames Tiger Studio sends and the printer answers. false when
 * not connected. */
bool k2_client_control(k2_client_t *c, pv_control_t cmd);
