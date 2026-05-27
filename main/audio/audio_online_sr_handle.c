#include "audio_online_sr_handle.h"

#include <stdbool.h>
#include <string.h>
#include "audio_decode.h"
#include "audio_encode.h"
#include "audio_init.h"
#include "audio_sr.h"
#include "bsp/bsp_ws.h"
#include "cJSON.h"
#include "com/com_debug.h"
#include "com/com_status.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#define ONLINE_AUDIO_RINGBUF_SIZE (64 * 1024)
#define ONLINE_AUDIO_TASK_STACK_SIZE (8 * 1024)
#define ONLINE_AUDIO_TASK_PRIORITY 5
#define ONLINE_WS_TASK_STACK_SIZE (8 * 1024)
#define ONLINE_WS_TASK_PRIORITY 5
#define ONLINE_OPUS_FRAME_MAX_SIZE 2048

static audio_online_sr_handle_t *s_online_handle = NULL;
static TaskHandle_t s_ws_task_handle = NULL;
static TaskHandle_t s_send_task_handle = NULL;
static volatile bool s_connecting = false;
static volatile bool s_drop_wakeup_reply = false;

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

static void audio_online_ws_text_cb(char *data, int len);
static void audio_online_ws_bin_cb(char *data, int len);
static void audio_online_send_task(void *arg);
static void audio_online_ws_task(void *arg);

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

static char *copy_ws_text(const char *data, int len)
{
    char *text = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_DEFAULT);
    if (text == NULL)
    {
        return NULL;
    }

    memcpy(text, data, len);
    text[len] = '\0';
    return text;
}

static void handle_hello_message(cJSON *json)
{
    cJSON *session_obj = cJSON_GetObjectItem(json, "session_id");
    const char *session_value = cJSON_GetStringValue(session_obj);
    if (session_value == NULL)
    {
        MY_LOGW("hello message has no session_id");
        return;
    }

    strlcpy(session_id, session_value, sizeof(session_id));
    MY_LOGI("WS session_id=%s", session_id);
    if (ws_event_group != NULL)
    {
        xEventGroupSetBits(ws_event_group, WEBSOCKET_SESSION_ID_BIT);
    }
}

static void handle_tts_message(cJSON *json)
{
    cJSON *state_obj = cJSON_GetObjectItem(json, "state");
    const char *state = cJSON_GetStringValue(state_obj);
    if (state == NULL)
    {
        return;
    }

    if (strcmp(state, "start") == 0)
    {
        if (!s_drop_wakeup_reply)
        {
            if (com_status == LISTENING && is_wsline)
            {
                bsp_ws_send_stop_listen();
            }
            com_status_change(SPEAKING);
        }
        return;
    }

    if (strcmp(state, "stop") == 0)
    {
        if (s_drop_wakeup_reply)
        {
            s_drop_wakeup_reply = false;
        }
        if (is_awake)
        {
            com_status_change(IDLE);
        }
    }
}

static void audio_online_ws_text_cb(char *data, int len)
{
    char *text = copy_ws_text(data, len);
    if (text == NULL)
    {
        MY_LOGE("copy WS text failed");
        return;
    }

    cJSON *json = cJSON_Parse(text);
    if (json == NULL)
    {
        MY_LOGW("invalid WS JSON: %s", text);
        free(text);
        return;
    }

    cJSON *type_obj = cJSON_GetObjectItem(json, "type");
    const char *type = cJSON_GetStringValue(type_obj);
    if (type == NULL)
    {
        cJSON_Delete(json);
        free(text);
        return;
    }

    if (strcmp(type, "hello") == 0)
    {
        handle_hello_message(json);
    }
    else if (strcmp(type, "tts") == 0)
    {
        handle_tts_message(json);
    }
    else if (strcmp(type, "stt") == 0)
    {
        MY_LOGI("WS STT: %s", text);
    }
    else if (strcmp(type, "llm") == 0)
    {
        MY_LOGI("WS LLM: %s", text);
    }
    else if (strcmp(type, "iot") == 0)
    {
        MY_LOGI("WS IoT: %s", text);
    }

    cJSON_Delete(json);
    free(text);
}

static void audio_online_ws_bin_cb(char *data, int len)
{
    if (s_drop_wakeup_reply)
    {
        return;
    }

    if (s_online_handle == NULL)
    {
        return;
    }

    esp_err_t ret = audio_online_sr_handle_write_data(s_online_handle, (const uint8_t *)data, len);
    if (ret != ESP_OK)
    {
        MY_LOGW("write online audio failed: %s", esp_err_to_name(ret));
    }
}

static void audio_online_send_task(void *arg)
{
    audio_online_sr_handle_t *handle = (audio_online_sr_handle_t *)arg;
    uint8_t *data = (uint8_t *)heap_caps_malloc(ONLINE_OPUS_FRAME_MAX_SIZE, MALLOC_CAP_SPIRAM);
    if (data == NULL)
    {
        MY_LOGE("alloc online send buffer failed");
        vTaskDelete(NULL);
        return;
    }

    while (true)
    {
        size_t read_size = 0;
        esp_err_t ret = audio_online_sr_handle_read_data(handle,
                                                         data,
                                                         ONLINE_OPUS_FRAME_MAX_SIZE,
                                                         &read_size);
        if (ret == ESP_OK && read_size > 0 && is_wsline && com_status == LISTENING)
        {
            bsp_ws_send_opus((char *)data, (int)read_size);
        }
    }
}

static void audio_online_ws_task(void *arg)
{
    (void)arg;

    if (!bsp_ws_is_connected())
    {
        bsp_ws_start();
    }

    if (bsp_ws_is_connected())
    {
        bsp_ws_send_hello();
        s_drop_wakeup_reply = true;
        bsp_ws_send_wakeup_word();
        if (is_awake)
        {
            com_status_change(IDLE);
        }
    }
    else
    {
        is_wsline = false;
        MY_LOGW("WebSocket connect failed");
    }

    s_connecting = false;
    s_ws_task_handle = NULL;
    vTaskDelete(NULL);
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

    ret = xTaskCreateWithCaps(audio_online_send_task,
                              "online_send_task",
                              ONLINE_AUDIO_TASK_STACK_SIZE,
                              handle,
                              ONLINE_AUDIO_TASK_PRIORITY,
                              &s_send_task_handle,
                              MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "create online send task failed");

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

esp_err_t audio_online_init(void)
{
    if (s_online_handle != NULL)
    {
        return ESP_OK;
    }

    s_online_handle = audio_online_sr_handle_create();
    ESP_RETURN_ON_FALSE(s_online_handle != NULL, ESP_ERR_NO_MEM, TAG, "create online handle failed");
    ESP_RETURN_ON_ERROR(audio_online_sr_handle_start(s_online_handle), TAG, "start online handle failed");

    bsp_ws_init();
    bsp_ws_set_callbacks(audio_online_ws_text_cb, audio_online_ws_bin_cb);

    return ESP_OK;
}

esp_err_t audio_online_start_async(void)
{
    if (s_online_handle == NULL)
    {
        ESP_RETURN_ON_ERROR(audio_online_init(), TAG, "online init failed");
    }

    if (s_connecting)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (bsp_ws_is_connected())
    {
        s_drop_wakeup_reply = true;
        bsp_ws_send_wakeup_word();
        return ESP_OK;
    }

    s_connecting = true;
    BaseType_t ret = xTaskCreateWithCaps(audio_online_ws_task,
                                         "online_ws_task",
                                         ONLINE_WS_TASK_STACK_SIZE,
                                         NULL,
                                         ONLINE_WS_TASK_PRIORITY,
                                         &s_ws_task_handle,
                                         MALLOC_CAP_SPIRAM);
    if (ret != pdPASS)
    {
        s_connecting = false;
        s_ws_task_handle = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool audio_online_is_ready(void)
{
    return is_wsline && bsp_ws_is_connected() && session_id[0] != '\0';
}
