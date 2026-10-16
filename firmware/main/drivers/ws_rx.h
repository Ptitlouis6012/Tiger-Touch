#pragma once

/* Reassembly of a WebSocket text message that arrives in chunks.
 *
 * esp_websocket_client hands a message over in pieces, each with its offset and
 * the message's total length. Every WebSocket driver needs the same few lines to
 * turn that back into one NUL-terminated string; this is that, once.
 *
 * The buffer lives in PSRAM and is grown, never shrunk: reports repeat and are
 * about the same size. */

#include <stdbool.h>
#include <stddef.h>

#include "esp_websocket_client.h"

typedef struct
{
    char *buf;
    size_t cap;
} ws_rx_t;

/* Feed it the event's data. Returns true when `*text` / `*len` now hold a whole
 * message, valid until the next call. Control frames, an empty message or one
 * longer than `max_len` are ignored (false). */
bool ws_rx_feed(ws_rx_t *rx, const esp_websocket_event_data_t *d, size_t max_len, const char **text,
                size_t *len);

void ws_rx_free(ws_rx_t *rx);
