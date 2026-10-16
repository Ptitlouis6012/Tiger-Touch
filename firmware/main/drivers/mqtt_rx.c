#include "mqtt_rx.h"

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

bool mqtt_rx_feed(mqtt_rx_t *rx, esp_mqtt_event_handle_t ev, size_t max_len, const char **text, size_t *len)
{
    if (ev->total_data_len <= 0 || (size_t)ev->total_data_len > max_len)
    {
        return false;
    }
    const size_t total = (size_t)ev->total_data_len;
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
    if ((size_t)ev->current_data_offset + (size_t)ev->data_len > total)
    {
        return false;
    }
    memcpy(rx->buf + ev->current_data_offset, ev->data, ev->data_len);
    if ((size_t)ev->current_data_offset + (size_t)ev->data_len != total)
    {
        return false; /* more chunks to come */
    }
    rx->buf[total] = '\0';
    *text = rx->buf;
    *len = total;
    return true;
}

void mqtt_rx_free(mqtt_rx_t *rx)
{
    free(rx->buf);
    rx->buf = NULL;
    rx->cap = 0;
}
