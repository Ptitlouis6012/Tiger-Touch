#pragma once

#include <stdbool.h>

#include "cloud.h"
#include "ui_dashboard.h"

/* One driver per way of reaching a printer. A driver turns a printer of the
 * account into the same `printer_view_t` the dashboard draws, so the screens
 * never learn which brand is behind them.
 *
 * Only one printer is watched at a time, which is what keeps the internal RAM
 * (Wi-Fi, TLS) within reach. */

typedef enum
{
    DRV_NONE = 0, /* this firmware cannot reach it (yet) */
    DRV_LAN,      /* on the local network */
    DRV_CLOUD,    /* through the maker's cloud */
} drv_kind_t;

typedef struct
{
    const char *brand; /* matches account_printer_t.brand */
    drv_kind_t (*support)(const account_printer_t *p);
    void *(*start)(const account_printer_t *p);
    void (*stop)(void *ctx);
    /* Fills `v` (name untouched). Cheap, called every second. */
    void (*snapshot)(void *ctx, printer_view_t *v);
    /* Optional: pause, resume or stop the print. NULL when the driver sends no
     * command. true when the command was handed to the connection; the printer's
     * own report says whether it was obeyed. */
    bool (*control)(void *ctx, pv_control_t cmd);
} printer_driver_t;

/* NULL when no driver can reach this printer. */
const printer_driver_t *drivers_for(const account_printer_t *p);

extern const printer_driver_t DRIVER_CREALITY;
extern const printer_driver_t DRIVER_BAMBU;
extern const printer_driver_t DRIVER_SNAPMAKER;
extern const printer_driver_t DRIVER_ELEGOO;      /* CC2: MQTT */
extern const printer_driver_t DRIVER_ELEGOO_SDCP; /* CC1: SDCP over WebSocket */
extern const printer_driver_t DRIVER_FLASHFORGE;
extern const printer_driver_t DRIVER_PRUSA;
extern const printer_driver_t DRIVER_ANYCUBIC;
extern const printer_driver_t DRIVER_KLIPPER;
extern const printer_driver_t DRIVER_OCTOPRINT;
