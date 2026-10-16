#pragma once

/* The small things every driver does with a parsed report, written once.
 *
 * Header-only: each is a few lines and the drivers each include only what they
 * use. Nothing here allocates or keeps state. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* Writes `*out` only when `key` is a number, so a report that leaves a field out
 * keeps the previous value instead of reading as zero. */
static inline void json_get_f(const cJSON *o, const char *key, float *out)
{
    const cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsNumber(v))
    {
        *out = (float)v->valuedouble;
    }
}

static inline void json_get_i(const cJSON *o, const char *key, int *out)
{
    const cJSON *v = cJSON_GetObjectItem(o, key);
    if (cJSON_IsNumber(v))
    {
        *out = (int)v->valuedouble;
    }
}

/* The string at `key`, or "" - never NULL, so it can go straight into strlcpy. */
static inline const char *json_str(const cJSON *o, const char *key)
{
    const cJSON *v = cJSON_GetObjectItem(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static inline const char *json_str_at(const cJSON *arr, int i)
{
    const cJSON *v = cJSON_GetArrayItem(arr, i);
    return cJSON_IsString(v) ? v->valuestring : "";
}

#define COLOR_UNKNOWN 0x5a5a5a

/* A filament colour as the makers write it, to 0xRRGGBB.
 *
 *   "#RRGGBB" / "RRGGBB"        the plain form
 *   "#0RRGGBB"                  Creality's, with a leading 0 where an alpha would sit
 *   "RRGGBBAA" / "#RRGGBBAA"    Snapmaker's: the colour is the first six digits
 *
 * An empty or missing string is "unknown": a grey that reads as "no data", not
 * black, which would read as a real spool. */
static inline uint32_t color_from_hex(const char *s)
{
    if (!s || !*s)
    {
        return COLOR_UNKNOWN;
    }
    if (*s == '#')
    {
        s++;
    }
    const size_t n = strlen(s);
    if (n == 7)
    {
        s++;
    }
    else if (n >= 8)
    {
        char six[7];
        memcpy(six, s, 6);
        six[6] = '\0';
        return (uint32_t)strtoul(six, NULL, 16) & 0xffffff;
    }
    return (uint32_t)strtoul(s, NULL, 16) & 0xffffff;
}
