#include "ota.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

static const char *TAG = "ota";

/* Where the manifest is published: the GitHub Pages site of the repository,
 * deployed by .github/workflows/pages.yml from the latest release. Devices in
 * the field read this address for as long as they live, so it never moves; a
 * new home is announced by a manifest published here. */
#define OTA_MANIFEST_URL "https://tigertag-project.github.io/Tiger-Touch/version.json"

/* Download block. In PSRAM: the internal RAM is what the TLS sockets need. */
#define OTA_CHUNK 4096

static bool s_probation;
/* A version that was installed and did not survive its probation: the
 * bootloader went back to the previous one. Offering it again would send the
 * owner round the same loop, so it is never offered again. The slot it sits in
 * stays marked invalid until something else is written there, so this is read
 * again at every start. */
static char s_refused[32];
/* Written by the background check, read by the settings menu. */
static portMUX_TYPE s_offer_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_have_offer;
static ota_offer_t s_offer;

/* ---- Probation ------------------------------------------------------------ */

void ota_boot(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY)
    {
        s_probation = true;
        ESP_LOGW(TAG, "new image on probation in %s: it is kept once the application is up", run->label);
    }
    ESP_LOGI(TAG, "running %s from %s", esp_app_get_description()->version, run ? run->label : "?");

    const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
    esp_app_desc_t d;
    if (bad && esp_ota_get_partition_description(bad, &d) == ESP_OK)
    {
        strlcpy(s_refused, d.version, sizeof s_refused);
        ESP_LOGW(TAG, "%s did not start and was rolled back: it is not offered again", s_refused);
    }
}

void ota_mark_healthy(void)
{
    if (!s_probation)
    {
        return;
    }
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
    {
        s_probation = false;
        ESP_LOGI(TAG, "image confirmed, rollback cancelled");
    }
}

bool ota_on_probation(void)
{
    return s_probation;
}

/* ---- Versions ------------------------------------------------------------- */

static bool parse_ver(const char *s, int v[3])
{
    return s && sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

/* >0 when a is newer than b. */
static int ver_cmp(const char *a, const char *b)
{
    int x[3], y[3];
    if (!parse_ver(a, x) || !parse_ver(b, y))
    {
        return 0;
    }
    for (int i = 0; i < 3; i++)
    {
        if (x[i] != y[i])
        {
            return x[i] > y[i] ? 1 : -1;
        }
    }
    return 0;
}

static bool is_hex64(const char *s)
{
    if (!s || strlen(s) != 64)
    {
        return false;
    }
    for (const char *p = s; *p; p++)
    {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
        {
            return false;
        }
    }
    return true;
}

/* ---- The manifest --------------------------------------------------------- */

typedef struct
{
    char buf[1024];
    size_t len;
} small_rx_t;

static esp_err_t on_manifest(esp_http_client_event_t *e)
{
    small_rx_t *r = (small_rx_t *)e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && r)
    {
        const size_t room = sizeof r->buf - 1 - r->len;
        const size_t n = (size_t)e->data_len < room ? (size_t)e->data_len : room;
        memcpy(r->buf + r->len, e->data, n);
        r->len += n;
        r->buf[r->len] = '\0';
    }
    return ESP_OK;
}

ota_check_t ota_check(ota_offer_t *out)
{
    small_rx_t *rx = heap_caps_calloc(1, sizeof *rx, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rx)
    {
        return OTA_CHECK_FAILED;
    }
    esp_http_client_config_t cfg = {
        .url = OTA_MANIFEST_URL,
        .event_handler = on_manifest,
        .user_data = rx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h)
    {
        free(rx);
        return OTA_CHECK_FAILED;
    }
    /* A cache in between must not answer with yesterday's manifest. */
    esp_http_client_set_header(h, "Cache-Control", "no-cache");
    const esp_err_t err = esp_http_client_perform(h);
    const int status = err == ESP_OK ? esp_http_client_get_status_code(h) : -1;
    esp_http_client_cleanup(h);
    if (status != 200)
    {
        ESP_LOGW(TAG, "manifest not read (%s, HTTP %d)", esp_err_to_name(err), status);
        free(rx);
        return OTA_CHECK_FAILED;
    }

    ota_check_t result = OTA_CHECK_FAILED;
    cJSON *root = cJSON_Parse(rx->buf);
    free(rx);
    const cJSON *ver = cJSON_GetObjectItem(root, "version");
    const cJSON *url = cJSON_GetObjectItem(root, "firmware_url");
    const cJSON *sha = cJSON_GetObjectItem(root, "firmware_sha256");
    int tmp[3];
    if (!cJSON_IsString(ver) || !cJSON_IsString(url) || !cJSON_IsString(sha) ||
        !parse_ver(ver->valuestring, tmp) || strncmp(url->valuestring, "https://", 8) != 0 ||
        strlen(url->valuestring) >= sizeof out->url || strlen(ver->valuestring) >= sizeof out->version ||
        !is_hex64(sha->valuestring))
    {
        /* No hash, no update: a manifest without one is not trusted at all. */
        ESP_LOGW(TAG, "manifest rejected: a field is missing or malformed");
    }
    else if (ver_cmp(ver->valuestring, esp_app_get_description()->version) > 0 &&
             strcmp(ver->valuestring, s_refused) != 0)
    {
        ota_offer_t o = {0};
        strlcpy(o.version, ver->valuestring, sizeof o.version);
        strlcpy(o.url, url->valuestring, sizeof o.url);
        strlcpy(o.sha256, sha->valuestring, sizeof o.sha256);
        if (out)
        {
            *out = o;
        }
        taskENTER_CRITICAL(&s_offer_mux);
        s_offer = o;
        s_have_offer = true;
        taskEXIT_CRITICAL(&s_offer_mux);
        result = OTA_NEWER;
        ESP_LOGI(TAG, "version %s is available", o.version);
    }
    else
    {
        taskENTER_CRITICAL(&s_offer_mux);
        s_have_offer = false;
        taskEXIT_CRITICAL(&s_offer_mux);
        result = OTA_UP_TO_DATE;
        ESP_LOGI(TAG, "up to date (published %s)", ver->valuestring);
    }
    cJSON_Delete(root);
    return result;
}

bool ota_cached_offer(ota_offer_t *out)
{
    taskENTER_CRITICAL(&s_offer_mux);
    const bool have = s_have_offer;
    if (have && out)
    {
        *out = s_offer;
    }
    taskEXIT_CRITICAL(&s_offer_mux);
    return have;
}

/* ---- The download --------------------------------------------------------- */

typedef struct
{
    const ota_offer_t *offer;
    ota_progress_cb cb;
    void *arg;
    esp_err_t result;
    SemaphoreHandle_t done;
} job_t;

/* The application description sits right after the image header and the first
 * segment header. Reading it from the first block refuses another product's
 * image (a TigerScale's, say) before a single byte is written. */
static bool image_is_ours(const uint8_t *data, size_t len, const char *want_version)
{
    const size_t at = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    if (len < at + sizeof(esp_app_desc_t))
    {
        return false;
    }
    esp_app_desc_t d;
    memcpy(&d, data + at, sizeof d);
    if (d.magic_word != ESP_APP_DESC_MAGIC_WORD)
    {
        ESP_LOGE(TAG, "not an application image");
        return false;
    }
    if (strncmp(d.project_name, esp_app_get_description()->project_name, sizeof d.project_name) != 0)
    {
        ESP_LOGE(TAG, "image of another product (%.32s)", d.project_name);
        return false;
    }
    if (strncmp(d.version, want_version, sizeof d.version) != 0)
    {
        ESP_LOGE(TAG, "image says %.32s, the manifest %s", d.version, want_version);
        return false;
    }
    return true;
}

static esp_err_t download(job_t *j)
{
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (!target)
    {
        ESP_LOGE(TAG, "no slot to update into");
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t *buf = heap_caps_malloc(OTA_CHUNK, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    mbedtls_sha256_context *sha = heap_caps_calloc(1, sizeof *sha, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf || !sha)
    {
        free(buf);
        free(sha);
        return ESP_ERR_NO_MEM;
    }

    /* A release asset is a redirect to a signed storage address several hundred
     * characters long: the request line has to hold it. */
    esp_http_client_config_t cfg = {
        .url = j->offer->url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .max_redirection_count = 5,
        .keep_alive_enable = false,
    };
    esp_err_t err = ESP_FAIL;
    esp_ota_handle_t ota = 0;
    bool ota_open = false;
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h)
    {
        goto out;
    }

    /* Follow the redirects by hand: the streaming API does not. */
    int status = 0;
    int64_t total = 0;
    for (int hop = 0; hop < 6; hop++)
    {
        err = esp_http_client_open(h, 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "cannot open the download: %s", esp_err_to_name(err));
            goto out;
        }
        total = esp_http_client_fetch_headers(h);
        status = esp_http_client_get_status_code(h);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
        {
            esp_http_client_set_redirection(h);
            esp_http_client_close(h);
            continue;
        }
        break;
    }
    if (status != 200)
    {
        ESP_LOGE(TAG, "download answered HTTP %d", status);
        err = ESP_FAIL;
        goto out;
    }
    if (total > 0 && total > (int64_t)target->size)
    {
        ESP_LOGE(TAG, "image of %lld bytes does not fit the %lu-byte slot", (long long)total,
                 (unsigned long)target->size);
        err = ESP_ERR_INVALID_SIZE;
        goto out;
    }

    mbedtls_sha256_init(sha);
    mbedtls_sha256_starts(sha, 0);
    int64_t got = 0;
    int last_pct = -1;
    for (;;)
    {
        /* The first block is read whole, so the application description is in
         * it; later ones are taken as they come. */
        int n = esp_http_client_read(h, (char *)buf, OTA_CHUNK);
        while (!ota_open && n > 0 && n < 512)
        {
            const int more = esp_http_client_read(h, (char *)buf + n, OTA_CHUNK - n);
            if (more <= 0)
            {
                break;
            }
            n += more;
        }
        if (n < 0)
        {
            ESP_LOGE(TAG, "read failed after %lld bytes", (long long)got);
            err = ESP_FAIL;
            goto out;
        }
        if (n == 0)
        {
            if (esp_http_client_is_complete_data_received(h))
            {
                break;
            }
            ESP_LOGE(TAG, "connection closed after %lld bytes", (long long)got);
            err = ESP_FAIL;
            goto out;
        }
        if (!ota_open)
        {
            if (!image_is_ours(buf, (size_t)n, j->offer->version))
            {
                err = ESP_ERR_INVALID_VERSION;
                goto out;
            }
            /* Erases the slot as it goes rather than all at once: a 6 MB erase
             * up front is a long silence on the screen. */
            err = esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &ota);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
                goto out;
            }
            ota_open = true;
        }
        mbedtls_sha256_update(sha, buf, (size_t)n);
        err = esp_ota_write(ota, buf, (size_t)n);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            goto out;
        }
        got += n;
        if (j->cb && total > 0)
        {
            const int pct = (int)(got * 100 / total);
            if (pct != last_pct)
            {
                last_pct = pct;
                j->cb(pct, j->arg);
            }
        }
    }

    uint8_t digest[32];
    mbedtls_sha256_finish(sha, digest);
    char hex[65];
    for (int i = 0; i < 32; i++)
    {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    if (strcmp(hex, j->offer->sha256) != 0)
    {
        /* Nothing is switched: the slot holds bytes nobody vouched for. */
        ESP_LOGE(TAG, "SHA-256 differs from the manifest; the image is not installed");
        err = ESP_ERR_INVALID_CRC;
        goto out;
    }

    err = esp_ota_end(ota); /* checks the image's own structure and digest */
    ota_open = false;
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        goto out;
    }
    err = esp_ota_set_boot_partition(target);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "%s installed in %s (%lld bytes); it boots next, on probation", j->offer->version,
                 target->label, (long long)got);
    }

out:
    if (ota_open)
    {
        esp_ota_abort(ota);
    }
    if (h)
    {
        esp_http_client_close(h);
        esp_http_client_cleanup(h);
    }
    mbedtls_sha256_free(sha);
    free(sha);
    free(buf);
    return err;
}

static void ota_task(void *arg)
{
    job_t *j = (job_t *)arg;
    j->result = download(j);
    xSemaphoreGive(j->done);
    vTaskDelete(NULL);
}

esp_err_t ota_install(const ota_offer_t *offer, ota_progress_cb cb, void *arg)
{
    if (!offer)
    {
        return ESP_ERR_INVALID_ARG;
    }
    job_t j = {.offer = offer, .cb = cb, .arg = arg, .result = ESP_FAIL};
    j.done = xSemaphoreCreateBinary();
    if (!j.done)
    {
        return ESP_ERR_NO_MEM;
    }
    /* Internal RAM, on purpose: esp_ota_write() disables the flash cache, and a
     * stack in PSRAM is not reachable while it is off. 10 KB holds the TLS
     * handshake; mbedTLS allocates its own buffers in PSRAM. */
    if (xTaskCreatePinnedToCore(ota_task, "ota", 10 * 1024, &j, 5, NULL, 1) != pdPASS)
    {
        ESP_LOGE(TAG, "could not start the update task");
        vSemaphoreDelete(j.done);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(j.done, portMAX_DELAY);
    vSemaphoreDelete(j.done);
    return j.result;
}
