#include "ws_rx.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

bool ws_rx_feed(ws_rx_t *rx, const esp_websocket_event_data_t *d, size_t max_len, const char **text,
                size_t *len)
{
    /* 0x1 is a text frame, 0x0 the continuation of one. */
    if (d->op_code != 0x1 && d->op_code != 0x0)
    {
        return false;
    }
    if (d->payload_len <= 0 || (size_t)d->payload_len > max_len)
    {
        return false;
    }
    const size_t total = (size_t)d->payload_len;
    if (total + 1 > rx->cap)
    {
        char *nb = heap_caps_realloc(rx->buf, total + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!nb)
        {
            return false;
        }
        rx->buf = nb;
        rx->cap = total + 1;
    }
    if ((size_t)d->payload_offset + (size_t)d->data_len > total)
    {
        return false;
    }
    memcpy(rx->buf + d->payload_offset, d->data_ptr, d->data_len);
    if ((size_t)d->payload_offset + (size_t)d->data_len != total)
    {
        return false; /* more chunks to come */
    }
    rx->buf[total] = '\0';
    *text = rx->buf;
    *len = total;
    return true;
}

void ws_rx_free(ws_rx_t *rx)
{
    free(rx->buf);
    rx->buf = NULL;
    rx->cap = 0;
}
