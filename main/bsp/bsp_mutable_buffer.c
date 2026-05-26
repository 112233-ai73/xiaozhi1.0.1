#include "bsp_mutable_buffer.h"
#include "esp_heap_caps.h"
#include <string.h>

struct mutable_buffer
{
    char *data;
    int data_len;
};

mutable_buffer_t *bsp_mutable_buffer_create(void)
{
    mutable_buffer_t *buffer = (mutable_buffer_t *)heap_caps_malloc(sizeof(mutable_buffer_t), MALLOC_CAP_SPIRAM);
    if (buffer == NULL)
    {
        return NULL;
    }

    memset(buffer, 0, sizeof(mutable_buffer_t));
    return buffer;
}

void bsp_mutable_buffer_add_data(mutable_buffer_t *buffer, void *data, size_t len)
{
    if (buffer == NULL || data == NULL || len == 0)
    {
        return;
    }

    char *data_buffer = NULL;
    if (buffer->data_len == 0)
    {
        data_buffer = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM);
    }
    else
    {
        data_buffer = (char *)heap_caps_realloc(buffer->data, buffer->data_len + len + 1, MALLOC_CAP_SPIRAM);
    }

    if (data_buffer == NULL)
    {
        return;
    }

    buffer->data = data_buffer;
    memcpy(buffer->data + buffer->data_len, data, len);
    buffer->data_len += len;
    buffer->data[buffer->data_len] = '\0';
}

void *bsp_mutable_buffer_get_data(mutable_buffer_t *buffer)
{
    if (buffer == NULL)
    {
        return NULL;
    }

    return buffer->data;
}

void bsp_mutable_buffer_free(mutable_buffer_t *buffer)
{
    if (buffer == NULL)
    {
        return;
    }

    if (buffer->data != NULL)
    {
        heap_caps_free(buffer->data);
        buffer->data = NULL;
        buffer->data_len = 0;
    }

    free(buffer);
}
