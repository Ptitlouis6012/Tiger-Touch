#include "wifi_mgr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";

#define BIT_GOT_IP BIT0
#define BIT_FAIL   BIT1

static EventGroupHandle_t s_ev;
static esp_timer_handle_t s_reconnect_timer;
static volatile bool s_connecting; /* inside wifi_mgr_connect() */
static volatile bool s_keep;       /* reconnect on its own once connected */
static volatile bool s_failed;     /* the last attempt failed */
static volatile bool s_retrying;   /* a caller will try again (wifi_mgr_set_retrying) */
static volatile int64_t s_lost_us; /* when a kept link went down, 0 while it is up */
#define LOST_GIVE_UP_US (60LL * 1000 * 1000) /* re-establishing longer than this reads as a failure */
static int s_retry;
static bool s_sntp_started;

static void reconnect_cb(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_ev, BIT_GOT_IP);
        if (s_connecting)
        {
            if (++s_retry < 3)
            {
                esp_wifi_connect();
            }
            else
            {
                s_connecting = false;
                xEventGroupSetBits(s_ev, BIT_FAIL);
            }
        }
        else if (s_keep)
        {
            if (!s_lost_us)
            {
                s_lost_us = esp_timer_get_time();
            }
            esp_timer_start_once(s_reconnect_timer, 3 * 1000 * 1000);
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        s_retry = 0;
        s_connecting = false;
        s_failed = false;
        s_lost_us = 0;
        xEventGroupSetBits(s_ev, BIT_GOT_IP);
    }
}

void wifi_mgr_init(void)
{
    s_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    const esp_timer_create_args_t ta = {.callback = reconnect_cb, .name = "wifi_rc"};
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_reconnect_timer));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* No modem sleep: the device is on mains power and holds several live
     * connections. Asleep between beacons, every packet waited for the next
     * one - up to 300 ms with a router that spaces them out - and on a weak
     * link TLS handshakes to the cloud timed out (2026-10-14: pings averaged
     * 740 ms, peaks of 2 s). */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

int wifi_mgr_scan(wifi_net_t *out, int max)
{
    wifi_scan_config_t sc = {0};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK)
    {
        return -1;
    }
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0)
    {
        return 0;
    }
    if (n > 40)
    {
        n = 40;
    }
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    if (!recs)
    {
        return -1;
    }
    esp_wifi_scan_get_ap_records(&n, recs);

    int count = 0;
    for (int i = 0; i < n && count < max; i++)
    {
        const char *ssid = (const char *)recs[i].ssid;
        if (!ssid[0])
        {
            continue; /* hidden network */
        }
        bool dup = false;
        for (int j = 0; j < count; j++)
        {
            if (strcmp(out[j].ssid, ssid) == 0)
            {
                dup = true;
                break;
            }
        }
        if (dup)
        {
            continue;
        }
        strlcpy(out[count].ssid, ssid, sizeof out[count].ssid);
        out[count].rssi = recs[i].rssi;
        out[count].secure = recs[i].authmode != WIFI_AUTH_OPEN;
        count++;
    }
    free(recs);

    /* strongest first */
    for (int i = 1; i < count; i++)
    {
        wifi_net_t t = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].rssi < t.rssi)
        {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = t;
    }
    return count;
}

esp_err_t wifi_mgr_connect(const char *ssid, const char *pass, uint32_t timeout_ms)
{
    wifi_config_t c = {0};
    strlcpy((char *)c.sta.ssid, ssid, sizeof c.sta.ssid);
    if (pass && pass[0])
    {
        strlcpy((char *)c.sta.password, pass, sizeof c.sta.password);
        c.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    }
    else
    {
        c.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    c.sta.pmf_cfg.capable = true;
    c.sta.pmf_cfg.required = false;
    /* The strongest access point of the network, not the first one heard: on
     * a mesh (several access points, one name) the default fast scan joined a
     * far one at -79 dBm while the device sat next to another. Every channel
     * is scanned, and every reconnection does it again. */
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    c.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    s_connecting = false;
    s_keep = false;
    esp_timer_stop(s_reconnect_timer);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(150));

    xEventGroupClearBits(s_ev, BIT_GOT_IP | BIT_FAIL);
    s_retry = 0;
    s_failed = false;
    s_connecting = true;
    esp_wifi_set_config(WIFI_IF_STA, &c);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_ev, BIT_GOT_IP | BIT_FAIL, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    s_connecting = false;
    if (bits & BIT_GOT_IP)
    {
        s_keep = true;
        ESP_LOGI(TAG, "connected to the network");
        return ESP_OK;
    }
    esp_wifi_disconnect();
    s_failed = true;
    ESP_LOGW(TAG, "could not connect to the network");
    return ESP_FAIL;
}

bool wifi_mgr_connected(void)
{
    return (xEventGroupGetBits(s_ev) & BIT_GOT_IP) != 0;
}

void wifi_mgr_set_retrying(bool retrying)
{
    s_retrying = retrying;
}

wifi_state_t wifi_mgr_state(void)
{
    if (wifi_mgr_connected())
    {
        return WIFI_STATE_CONNECTED;
    }
    const bool reestablishing = s_keep && (!s_lost_us || esp_timer_get_time() - s_lost_us < LOST_GIVE_UP_US);
    if (s_connecting || s_retrying || reestablishing)
    {
        return WIFI_STATE_CONNECTING; /* trying, or re-establishing a lost link */
    }
    return (s_failed || s_keep) ? WIFI_STATE_FAILED : WIFI_STATE_DOWN;
}

int wifi_mgr_rssi(void)
{
    if (!wifi_mgr_connected())
    {
        return 0;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
    {
        return 0;
    }
    return ap.rssi;
}

bool wifi_mgr_country(char out[3])
{
    out[0] = '\0';
    /* Two letters plus room: the driver writes the third octet as well. */
    char cc[4] = {0};
    if (esp_wifi_get_country_code(cc) != ESP_OK || !isalpha((unsigned char)cc[0]) || !isalpha((unsigned char)cc[1]))
    {
        return false; /* "01" is the world-safe default: the access point said nothing */
    }
    out[0] = (char)toupper((unsigned char)cc[0]);
    out[1] = (char)toupper((unsigned char)cc[1]);
    out[2] = '\0';
    return true;
}

bool wifi_mgr_sync_time(uint32_t timeout_ms)
{
    if (!s_sntp_started)
    {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        if (esp_netif_sntp_init(&cfg) != ESP_OK)
        {
            return false;
        }
        s_sntp_started = true;
    }
    if (time(NULL) > 1700000000)
    {
        return true;
    }
    return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
}

bool wifi_mgr_info(wifi_link_info_t *o)
{
    memset(o, 0, sizeof *o);
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        snprintf(o->mac, sizeof o->mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
                 mac[5]);
    }
    if (!wifi_mgr_connected())
    {
        return false;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
    {
        return false;
    }
    strlcpy(o->ssid, (const char *)ap.ssid, sizeof o->ssid);
    o->rssi = ap.rssi;
    o->channel = ap.primary;
    esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (n && esp_netif_get_ip_info(n, &ip) == ESP_OK)
    {
        snprintf(o->ip, sizeof o->ip, IPSTR, IP2STR(&ip.ip));
    }
    return true;
}
