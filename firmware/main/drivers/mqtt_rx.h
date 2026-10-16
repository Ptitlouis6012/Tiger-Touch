#pragma once

/* Reassembly of an MQTT message that arrives in chunks.
 *
 * esp-mqtt hands a long message over in pieces, each with its offset and the
 * message's total length. Every MQTT driver needs the same few lines to turn
 * that back into one NUL-terminated string; this is that, once.
 *
 * The buffer lives in PSRAM and is grown, never shrunk: reports repeat and are
 * about the same size. */

#include <stdbool.h>
#include <stddef.h>

#include "mqtt_client.h"

typedef struct
{
    char *buf;
    size_t cap;
} mqtt_rx_t;

/* Feed it a MQTT_EVENT_DATA event. Returns true when `*text` / `*len` now hold a
 * whole message, valid until the next call. An empty message, or one longer than
 * `max_len`, is ignored (false). The topic is only present on the first chunk:
 * a caller that needs it reads it when `ev->current_data_offset == 0`. */
bool mqtt_rx_feed(mqtt_rx_t *rx, esp_mqtt_event_handle_t ev, size_t max_len, const char **text, size_t *len);

void mqtt_rx_free(mqtt_rx_t *rx);
