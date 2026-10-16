#pragma once

#include <stdbool.h>

#include "esp_err.h"

/* Over-the-air update.
 *
 * The device reads one small manifest (version.json) published with each
 * release, and installs the image it names into the other application slot. The
 * manifest, the image and every redirect between them are fetched over TLS
 * verified against the certificate bundle; the image's SHA-256 must equal the
 * manifest's, its application name must be this firmware's and its version the
 * one announced, or nothing is switched. A new image boots once on probation:
 * the bootloader returns to the previous one unless the new one calls
 * ota_mark_healthy(). See docs/OTA.md. */

typedef struct
{
    char version[16];  /* "X.Y.Z" */
    char url[256];     /* the application image */
    char sha256[65];   /* its SHA-256, lowercase hex */
} ota_offer_t;

typedef enum
{
    OTA_CHECK_FAILED = -1, /* not reachable, or a manifest that does not parse */
    OTA_UP_TO_DATE = 0,
    OTA_NEWER = 1,
} ota_check_t;

/* Says, once at start-up, whether this image is on probation. Call it before
 * anything that could crash. */
void ota_boot(void);

/* The running image works: keep it. Called once the application has reached a
 * screen that waits for a person; harmless to call again. */
void ota_mark_healthy(void);

/* True while this image has not been confirmed yet (it was just installed). */
bool ota_on_probation(void);

/* Reads the manifest. `out` is filled for OTA_NEWER only. Blocking, a few
 * seconds at most; needs Wi-Fi and the clock. */
ota_check_t ota_check(ota_offer_t *out);

/* What the last check found, for the settings menu: true and the offer when a
 * newer version was seen. */
bool ota_cached_offer(ota_offer_t *out);

/* Called from the download task with the share written so far, 0-100. */
typedef void (*ota_progress_cb)(int percent, void *arg);

/* Downloads, checks and installs `offer`, then sets it to boot next. Blocking;
 * the work runs in a task of its own whose stack is in internal RAM, because a
 * flash write must not run on a stack the cache can lose. The caller restarts. */
esp_err_t ota_install(const ota_offer_t *offer, ota_progress_cb cb, void *arg);
