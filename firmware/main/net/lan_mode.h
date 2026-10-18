#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Live view (Settings > Live view), as on TigerScale V3: Tiger Studio Manager, on
 * the same network, sees the screen live, touches it and takes screenshots of it
 * (for an issue report or a post). There is no web page: Studio is the client.
 *
 * The owner's decision (2026-10-18). Every request carries the access code: six
 * characters drawn on the device, kept across restarts and updates, shown in the
 * settings and written to the owner's account (the presence document, which
 * only the owner reads). Wrong codes are counted: after ten, nothing is
 * accepted for a minute. Live view is on by default; switched off, nothing is
 * served at all. A touch from Studio is a touch on the screen, nothing more:
 * stopping a print still asks for the confirmation. */

/* Reads the code and the switch, draws a code the first time, and starts the
 * server when live view is on. After wifi_mgr_init(). */
void lan_mode_init(void);

bool lan_mode_live(void);
/* Saved; the server starts or stops at once. */
void lan_mode_set_live(bool on);

/* The six characters. */
const char *lan_mode_code(void);
/* Draws a new code and saves it: a page open with the old one stops at once. */
void lan_mode_new_code(void);

/* Changes whenever the code or the switch does: tells the heartbeat to say so. */
uint32_t lan_mode_version(void);
