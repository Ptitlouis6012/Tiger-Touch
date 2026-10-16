#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Small string store on top of NVS (namespace "tiger"). */

void settings_init(void);

/* Returns true when the key exists and is not empty; `out` is always valid. */
bool settings_get(const char *key, char *out, size_t out_size);
bool settings_set(const char *key, const char *value);
void settings_erase(const char *key);

/* A binary record. get: true when the key holds exactly `size` bytes, read
 * into `out`. set: written only when it differs from what is stored, so a
 * record that does not change costs no flash wear. */
bool settings_get_blob(const char *key, void *out, size_t size);
bool settings_set_blob(const char *key, const void *data, size_t size);

/* Erases the whole NVS partition: Wi-Fi, account, language, choices, and the
 * Wi-Fi driver's own records. Nothing in RAM is touched; restart right after.
 * The next boot finds an empty flash and runs the first-start setup. */
void settings_factory_reset(void);
