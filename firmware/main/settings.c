#include "settings.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NS "tiger"

static const char *TAG = "settings";

void settings_init(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS needs erasing (%s)", esp_err_to_name(e));
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(e));
    }
}

bool settings_get(const char *key, char *out, size_t out_size)
{
    out[0] = '\0';
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
    {
        return false;
    }
    size_t len = out_size;
    esp_err_t e = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    if (e != ESP_OK)
    {
        out[0] = '\0';
        return false;
    }
    return out[0] != '\0';
}

bool settings_set(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
    {
        return false;
    }
    esp_err_t e = nvs_set_str(h, key, value ? value : "");
    if (e == ESP_OK)
    {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e == ESP_OK;
}

bool settings_get_blob(const char *key, void *out, size_t size)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK)
    {
        return false;
    }
    size_t len = 0;
    esp_err_t e = nvs_get_blob(h, key, NULL, &len);
    if (e == ESP_OK && len == size)
    {
        e = nvs_get_blob(h, key, out, &len);
    }
    nvs_close(h);
    return e == ESP_OK && len == size;
}

bool settings_set_blob(const char *key, const void *data, size_t size)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
    {
        return false;
    }
    /* Unchanged: nothing to write. */
    size_t len = 0;
    bool same = false;
    if (nvs_get_blob(h, key, NULL, &len) == ESP_OK && len == size)
    {
        void *old = malloc(size);
        same = old && nvs_get_blob(h, key, old, &len) == ESP_OK && memcmp(old, data, size) == 0;
        free(old);
    }
    esp_err_t e = ESP_OK;
    if (!same)
    {
        e = nvs_set_blob(h, key, data, size);
        if (e == ESP_OK)
        {
            e = nvs_commit(h);
        }
        if (e != ESP_OK)
        {
            ESP_LOGW(TAG, "%s not stored: %s", key, esp_err_to_name(e));
        }
    }
    nvs_close(h);
    return e == ESP_OK;
}

void settings_erase(const char *key)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK)
    {
        return;
    }
    nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

void settings_factory_reset(void)
{
    /* Close before erasing; the partition is then empty for the next boot,
     * where settings_init() finds it blank and initialises it afresh. */
    nvs_flash_deinit();
    esp_err_t e = nvs_flash_erase();
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "factory reset: erase failed: %s", esp_err_to_name(e));
    }
}
