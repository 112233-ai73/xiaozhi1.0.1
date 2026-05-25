#include "audio_online_sr_handle.h"

#include <stdbool.h>
#include <string.h>
#include "audio_decode.h"
#include "audio_encode.h"
#include "audio_init.h"
#include "audio_sr.h"
#include "com/com_debug.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#define ONLINE_AUDIO_RINGBUF_SIZE (64 * 1024)
#define ONLINE_AUDIO_TASK_STACK_SIZE (8 * 1024)
#define ONLINE_AUDIO_TASK_PRIORITY 5

struct audio_online_sr_handle
{
    audio_encode_t *audio_encode;
    audio_decode_t *audio_decode;

    RingbufHandle_t encode_input_buffer;
    RingbufHandle_t encode_output_buffer;
    RingbufHandle_t decode_input_buffer;
    RingbufHandle_t decode_output_buffer;

    TaskHandle_t play_task_handle;
    bool started;
};

static void audio_online_play_task(void *arg)
{
    audio_online_sr_handle_t *handle = (audio_online_sr_handle_t *)arg;
    size_t len = 0;

    while (true)
    {
        uint8_t *data = (uint8_t *)xRingbufferReceive(handle->decode_output_buffer, &len, portMAX_DELAY);
        if (data == NULL || len == 0)
        {
            continue;
        }

        audio_write(data, (int)len);
        vRingbufferReturnItem(handle->decode_output_buffer, data);
    }
}

audio_online_sr_handle_t *audio_online_sr_handle_create(void)
{
    audio_online_sr_handle_t *handle =
        (audio_online_sr_handle_t *)heap_caps_calloc(1, sizeof(audio_online_sr_handle_t), MALLOC_CAP_SPIRAM);
    if (handle == NULL)
    {
        return NULL;
    }

    handle->audio_encode = audio_encode_create();
    handle->audio_decode = audio_decode_create();
    handle->encode_output_buffer =
        xRingbufferCreateWithCaps(ONLINE_AUDIO_RINGBUF_SIZE, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    handle->decode_input_buffer =
        xRingbufferCreateWithCaps(ONLINE_AUDIO_RINGBUF_SIZE, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    handle->decode_output_buffer =
        xRingbufferCreateWithCaps(ONLINE_AUDIO_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);

    if (handle->audio_encode == NULL ||
        handle->audio_decode == NULL ||
        handle->encode_output_buffer == NULL ||
        handle->decode_input_buffer == NULL ||
        handle->decode_output_buffer == NULL)
    {
        MY_LOGE("create online audio handle failed");
        return handle;
    }

    return handle;
}

esp_err_t audio_online_sr_handle_start(audio_online_sr_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is null");
    ESP_RETURN_ON_FALSE(!handle->started, ESP_ERR_INVALID_STATE, TAG, "online audio already started");
    ESP_RETURN_ON_FALSE(handle->audio_encode != NULL, ESP_ERR_INVALID_STATE, TAG, "encoder is null");
    ESP_RETURN_ON_FALSE(handle->audio_decode != NULL, ESP_ERR_INVALID_STATE, TAG, "decoder is null");
    ESP_RETURN_ON_FALSE(handle->encode_output_buffer != NULL, ESP_ERR_INVALID_STATE, TAG, "encode output buffer is null");
    ESP_RETURN_ON_FALSE(handle->decode_input_buffer != NULL, ESP_ERR_INVALID_STATE, TAG, "decode input buffer is null");
    ESP_RETURN_ON_FALSE(handle->decode_output_buffer != NULL, ESP_ERR_INVALID_STATE, TAG, "decode output buffer is null");

    handle->encode_input_buffer = app_sr_get_audio_ringbuf();
    ESP_RETURN_ON_FALSE(handle->encode_input_buffer != NULL,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "SR audio ringbuffer is null, call app_sr_start first");

    audio_encode_register_input_ringbuf(handle->audio_encode, handle->encode_input_buffer);
    audio_encode_register_output_ringbuf(handle->audio_encode, handle->encode_output_buffer);
    audio_decode_register_input_ringbuf(handle->audio_decode, handle->decode_input_buffer);
    audio_decode_register_output_ringbuf(handle->audio_decode, handle->decode_output_buffer);

    audio_decode_start(handle->audio_decode);
    audio_encode_start(handle->audio_encode);

    BaseType_t ret = xTaskCreateWithCaps(audio_online_play_task,
                                         "online_play_task",
                                         ONLINE_AUDIO_TASK_STACK_SIZE,
                                         handle,
                                         ONLINE_AUDIO_TASK_PRIORITY,
                                         &handle->play_task_handle,
                                         MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "create online play task failed");

    handle->started = true;
    return ESP_OK;
}

esp_err_t audio_online_sr_handle_read_data(audio_online_sr_handle_t *handle,
                                           uint8_t *data,
                                           size_t data_size,
                                           size_t *read_size)
{
    ESP_RETURN_ON_FALSE(handle != NULL && data != NULL && read_size != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "invalid read args");
    ESP_RETURN_ON_FALSE(handle->encode_output_buffer != NULL,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "encode output buffer is null");

    size_t item_size = 0;
    uint8_t *item = (uint8_t *)xRingbufferReceive(handle->encode_output_buffer, &item_size, portMAX_DELAY);
    ESP_RETURN_ON_FALSE(item != NULL, ESP_FAIL, TAG, "read encoded data failed");

    if (item_size > data_size)
    {
        vRingbufferReturnItem(handle->encode_output_buffer, item);
        *read_size = 0;
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(data, item, item_size);
    *read_size = item_size;
    vRingbufferReturnItem(handle->encode_output_buffer, item);

    return ESP_OK;
}

esp_err_t audio_online_sr_handle_write_data(audio_online_sr_handle_t *handle,
                                            const uint8_t *data,
                                            size_t size)
{
    ESP_RETURN_ON_FALSE(handle != NULL && data != NULL && size > 0,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "invalid write args");
    ESP_RETURN_ON_FALSE(handle->decode_input_buffer != NULL,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "decode input buffer is null");

    BaseType_t ret = xRingbufferSend(handle->decode_input_buffer, data, size, pdMS_TO_TICKS(100));
    return ret == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
