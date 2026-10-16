#pragma once

#include <stddef.h>

#include "esp_http_client.h"

/* One blocking HTTP request to a printer on the local network, for the drivers
 * that poll (FlashForge, Prusa, Klipper, OctoPrint). The answer's body is
 * copied into `out`. */

typedef struct
{
    const char *url;
    esp_http_client_method_t method;
    const char *body;         /* NULL for none */
    const char *content_type; /* with a body; default application/json */
    const char *user;         /* HTTP Digest credentials, NULL for none */
    const char *pass;
    const char *api_key;      /* sent as X-Api-Key when not empty (OctoPrint, Moonraker, PrusaLink's legacy key) */
    int timeout_ms;           /* 0 = 5000 */
} http_lite_req_t;

/* Returns the HTTP status, or -1 when the request could not be made. `out` is
 * always NUL-terminated and cut at cap-1. */
int http_lite(const http_lite_req_t *r, char *out, size_t cap);
