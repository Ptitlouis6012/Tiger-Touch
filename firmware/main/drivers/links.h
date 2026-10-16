#pragma once

#include <stdbool.h>

#include "cloud.h"
#include "printer_driver.h"

/* Which printers stay connected, and what that costs.
 *
 * The limit is the internal RAM, not a count of printers: every connection
 * holds a task stack and socket buffers there, whatever it is. A printer is
 * therefore charged the load slots its connection really takes, one slot being
 * one kilobyte, and the device refuses the one that would not fit at the moment
 * somebody switches it on, rather than leaving it silently offline later. The
 * same model as the TigerSpool's printer_budget.h.
 *
 * A switched-on printer keeps its link open and its last report in memory, so
 * showing it is a copy, not a connection. */

/* The printers of the account live in the caller's array; this module reads
 * it. Whoever rewrites that array holds links_lock() while doing so. */
void links_init(account_printer_t *printers, const int *count);
void links_lock(void);
void links_unlock(void);

/* After the list was rewritten: reload the switched-on set from the settings,
 * and let go of the links that no longer match a printer. */
void links_reload(void);

/* Is the printer switched on. A printer with no driver never is. */
bool links_enabled(int idx);

/* Can this firmware reach it at all (a switch is only offered then). */
bool links_supported(int idx);

/* Switches a printer on or off. Switching off is always granted; switching on
 * is refused (false) when the load budget has no room for it. */
bool links_set_enabled(int idx, bool on);

/* The brand of the printer at `idx` ("bambulab"...), or "" past the end. */
const char *links_brand(int idx);

/* The share of the budget the switched-on printers use, in percent. */
int links_used_pct(void);

/* The last report of a switched-on printer. false when it has no link (yet).
 * `settled` is true once it answers, or has said why it cannot. */
bool links_view(int idx, printer_view_t *v, bool *settled);

/* Sends pause, resume or stop to a switched-on printer. false when it has no link
 * or its driver takes no command. */
bool links_control(int idx, pv_control_t cmd);

/* Called about once a second: opens at most one missing link (the one with
 * id `prefer_id` first), closes the ones switched off, refreshes every report. */
/* No link opens before this is true: the network is up and the account, when
 * there is one, has answered. A cloud printer reads its session at start, and a
 * link opened before would sit silent until the next reload. */
void links_set_network_ready(bool ready);

void links_tick(const char *prefer_id);
