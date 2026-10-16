#include "http_lite.h"

#include <string.h>

typedef struct
{
    char *buf;
    size_t cap;
    size_t len;
} sink_t;

static esp_err_t on_http(esp_http_client_event_t *e)
{
    sink_t *s = (sink_t *)e->user_data;
    if (!s)
    {
        return ESP_OK;
    }
    switch (e->event_id)
    {
    case HTTP_EVENT_HEADERS_SENT:
        /* A Digest exchange answers 401 first: only the last answer counts. */
        s->len = 0;
        s->buf[0] = '\0';
        break;
    case HTTP_EVENT_ON_DATA:
    {
        const size_t room = s->cap - 1 - s->len;
        const size_t n = (size_t)e->data_len < room ? (size_t)e->data_len : room;
        if (n)
        {
            memcpy(s->buf + s->len, e->data, n);
            s->len += n;
            s->buf[s->len] = '\0';
        }
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

int http_lite(const http_lite_req_t *r, char *out, size_t cap)
{
    if (cap == 0)
    {
        return -1;
    }
    out[0] = '\0';
    sink_t sink = {.buf = out, .cap = cap, .len = 0};

    esp_http_client_config_t cfg = {
        .url = r->url,
        .method = r->method,
        .timeout_ms = r->timeout_ms > 0 ? r->timeout_ms : 5000,
        .event_handler = on_http,
        .user_data = &sink,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
        .keep_alive_enable = false,
    };
    if (r->user && r->user[0])
    {
        cfg.username = r->user;
        cfg.password = r->pass ? r->pass : "";
        cfg.auth_type = HTTP_AUTH_TYPE_DIGEST;
    }
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h)
    {
        return -1;
    }
    if (r->api_key && r->api_key[0])
    {
        esp_http_client_set_header(h, "X-Api-Key", r->api_key);
    }
    if (r->body)
    {
        esp_http_client_set_header(h, "Content-Type", r->content_type ? r->content_type : "application/json");
        esp_http_client_set_post_field(h, r->body, (int)strlen(r->body));
    }
    const esp_err_t err = esp_http_client_perform(h);
    const int status = err == ESP_OK ? esp_http_client_get_status_code(h) : -1;
    esp_http_client_cleanup(h);
    return status;
}
