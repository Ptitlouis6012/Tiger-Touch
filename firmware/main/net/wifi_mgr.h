#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    char ssid[33];
    int8_t rssi;
    bool secure;
} wifi_net_t;

void wifi_mgr_init(void);

/* Blocking scan. Returns the number of distinct networks written to `out`
 * (strongest first), or -1 on failure. */
int wifi_mgr_scan(wifi_net_t *out, int max);

/* Blocking connect. `pass` may be NULL or empty for an open network.
 * ESP_OK once an address was obtained. After a success the link is kept up
 * and re-established on its own. */
esp_err_t wifi_mgr_connect(const char *ssid, const char *pass, uint32_t timeout_ms);

bool wifi_mgr_connected(void);

/* What the Wi-Fi icon shows. CONNECTING while an attempt runs, while a lost
 * link is being re-established on its own, and while a caller says it keeps
 * trying (wifi_mgr_set_retrying); FAILED once an attempt failed and nobody is
 * trying; DOWN when nothing was ever tried (no network set up). */
typedef enum
{
    WIFI_STATE_DOWN,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
} wifi_state_t;
wifi_state_t wifi_mgr_state(void);

/* A caller that will try again after a failed attempt (the start-up in the
 * background) says so: the pause between two attempts then still reads as
 * "connecting", not as a failure. */
void wifi_mgr_set_retrying(bool retrying);

/* Signal strength of the access point in dBm, 0 when not connected. */
int wifi_mgr_rssi(void);

/* TLS certificate checks need the wall clock. Blocks until SNTP answered or
 * the timeout expired; true when the clock is set. The clock is UTC: the time
 * zone is net/tz.c's business. */
bool wifi_mgr_sync_time(uint32_t timeout_ms);

/* The country the access point announces (802.11d), two capital letters. False
 * when not connected or when it announced none. Only meaningful while connected:
 * the driver forgets it on disconnect. */
bool wifi_mgr_country(char out[3]);

typedef struct
{
    char ssid[33];
    int rssi;
    int channel;
    char ip[16];
    char mac[18];
} wifi_link_info_t;

/* Fills what is known; false when not connected (the MAC is always given). */
bool wifi_mgr_info(wifi_link_info_t *out);
