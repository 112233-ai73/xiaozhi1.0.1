#include "audio_sr.h"

static const char *TAG = "AUDIO_SR";

static model_iface_data_t *model_data = NULL;
static const esp_mn_iface_t *multinet = NULL;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static QueueHandle_t g_result_que = NULL;
static srmodel_list_t *models = NULL;
static RingbufHandle_t afe_rb_1 = NULL;
static RingbufHandle_t afe_rb_2 = NULL;

#define MULTINET_TIMEOUT_MS 500
#define AFE_RINGBUF_FRAME_NUM 8

static void audio_feed_task(void *arg)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;

    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    size_t feed_size = feed_chunksize * feed_nch * sizeof(int16_t);
    int16_t *feed_buff = (int16_t *)malloc(feed_size);
    if (feed_buff == NULL)
    {
        ESP_LOGE(TAG, "malloc feed buffer failed");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        audio_read(feed_buff, feed_size);
        afe_handle->feed(afe_data, feed_buff);
    }
}

static void audio_detect_task(void *pvParam)
{
    com_status_change(IDLE);
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)pvParam;

    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);

    while (true)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            ESP_LOGE(TAG, "fetch error!");
            continue;
        }

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            ESP_LOGI(TAG, "Wakeword detected");

            sr_result_t result = {0};
            result.wakenet_mode = WAKENET_DETECTED;
            result.state = ESP_MN_STATE_DETECTING;
            result.command_id = 0;

            xQueueSend(g_result_que, &result, 10);
            continue;
        }
        else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            ESP_LOGI(TAG, "Channel verified");
            com_status_change(WORKING);
            continue;
        }

        if (com_status == LISTERENING && res->vad_state == VAD_SPEECH)
        {
            int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
            int feed_nch = afe_handle->get_feed_channel_num(afe_data);
            size_t feed_size = feed_chunksize * feed_nch * sizeof(int16_t);

            if (afe_rb_1 == NULL)
            {
                afe_rb_1 = xRingbufferCreate(feed_size * AFE_RINGBUF_FRAME_NUM, RINGBUF_TYPE_BYTEBUF);
                if (afe_rb_1 == NULL)
                {
                    ESP_LOGE(TAG, "create afe_rb_1 failed");
                    continue;
                }
            }

            if (afe_rb_2 == NULL)
            {
                afe_rb_2 = xRingbufferCreate(feed_size * AFE_RINGBUF_FRAME_NUM, RINGBUF_TYPE_BYTEBUF);
                if (afe_rb_2 == NULL)
                {
                    ESP_LOGE(TAG, "create afe_rb_2 failed");
                    continue;
                }
            }

            if (res->data)
            {
                size_t afe_data_len =
                    afe_handle->get_fetch_chunksize(afe_data) * sizeof(int16_t);

                xRingbufferSend(afe_rb_1, res->data, afe_data_len, pdMS_TO_TICKS(20));
                xRingbufferSend(afe_rb_2, res->data, afe_data_len, pdMS_TO_TICKS(20));
            }
        }
    }
}

static void audio_multinet_task(void *arg)
{
    (void)arg;

    const size_t mn_chunk_bytes = multinet->get_samp_chunksize(model_data) * sizeof(int16_t);

    while (true)
    {
        size_t item_size = 0;
        int16_t *item = (int16_t *)xRingbufferReceive(afe_rb_1, &item_size, portMAX_DELAY);
        if (item == NULL)
        {
            continue;
        }

        TickType_t start_tick = xTaskGetTickCount();
        esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;

        while (true)
        {
            if (item_size >= mn_chunk_bytes)
            {
                mn_state = multinet->detect(model_data, item);
            }
            vRingbufferReturnItem(afe_rb_1, item);
            item = NULL;

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                if (mn_result != NULL && mn_result->num > 0)
                {
                    sr_result_t result = {0};
                    result.wakenet_mode = WAKENET_NO_DETECT;
                    result.state = ESP_MN_STATE_DETECTED;
                    result.command_id = mn_result->command_id[0];
                    result.confidence = mn_result->prob[0];

                    ESP_LOGI(TAG, "command detected: id=%d, confidence=%.3f",
                             result.command_id, result.confidence);
                    xQueueOverwrite(g_result_que, &result);
                }
                break;
            }

            TickType_t elapsed_tick = xTaskGetTickCount() - start_tick;
            if (elapsed_tick >= pdMS_TO_TICKS(MULTINET_TIMEOUT_MS) ||
                mn_state == ESP_MN_STATE_TIMEOUT)
            {
                ESP_LOGW(TAG, "command detect timeout");
                multinet->clean(model_data);
                break;
            }

            TickType_t remain_tick = pdMS_TO_TICKS(MULTINET_TIMEOUT_MS) - elapsed_tick;
            item = (int16_t *)xRingbufferReceive(afe_rb_1, &item_size, remain_tick);
            if (item == NULL)
            {
                ESP_LOGW(TAG, "command detect timeout");
                multinet->clean(model_data);
                break;
            }
        }
    }
}

esp_err_t app_sr_start(void)
{
    g_result_que = xQueueCreate(1, sizeof(sr_result_t));
    ESP_RETURN_ON_FALSE(NULL != g_result_que, ESP_ERR_NO_MEM, TAG, "Failed create result queue");

    models = esp_srmodel_init("model");
    ESP_RETURN_ON_FALSE(NULL != models, ESP_FAIL, TAG, "Failed init SR models");

    afe_config_t *afe_config = afe_config_init("MM", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    ESP_RETURN_ON_FALSE(NULL != afe_config, ESP_ERR_NO_MEM, TAG, "Failed create AFE config");

    afe_config->wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config->aec_init = false;

    afe_handle = esp_afe_handle_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_handle, ESP_FAIL, TAG, "Failed create AFE handle");

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_data, ESP_FAIL, TAG, "Failed create AFE data");
    ESP_LOGI(TAG, "load wakenet:%s", afe_config->wakenet_model_name);

    char *mn_name = esp_srmodel_filter(models, ESP_MN_CHINESE, NULL);
    if (mn_name == NULL)
    {
        ESP_LOGE(TAG, "No multinet model found");
        return ESP_FAIL;
    }

    multinet = esp_mn_handle_from_name(mn_name);
    ESP_RETURN_ON_FALSE(NULL != multinet, ESP_FAIL, TAG, "Failed create multinet handle");

    model_data = multinet->create(mn_name, 5760);
    ESP_RETURN_ON_FALSE(NULL != model_data, ESP_FAIL, TAG, "Failed create multinet data");
    ESP_LOGI(TAG, "load multinet:%s", mn_name);

    size_t mn_chunk_bytes = multinet->get_samp_chunksize(model_data) * sizeof(int16_t);
    afe_rb_1 = xRingbufferCreate(mn_chunk_bytes * AFE_RINGBUF_FRAME_NUM, RINGBUF_TYPE_BYTEBUF);
    ESP_RETURN_ON_FALSE(NULL != afe_rb_1, ESP_ERR_NO_MEM, TAG, "Failed create afe_rb_1");

    afe_rb_2 = xRingbufferCreate(mn_chunk_bytes * AFE_RINGBUF_FRAME_NUM, RINGBUF_TYPE_BYTEBUF);
    ESP_RETURN_ON_FALSE(NULL != afe_rb_2, ESP_ERR_NO_MEM, TAG, "Failed create afe_rb_2");

    esp_mn_commands_clear();
    for (int i = 0; i < command_word_count; i++)
    {
        esp_mn_commands_add(i, cmd_phoneme[i]);
    }

    esp_mn_commands_update();
    esp_mn_commands_print();
    multinet->print_active_speech_commands(model_data);

    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "Feed Task", 4 * 1024, afe_data, 5, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "Detect Task", 6 * 1024, afe_data, 5, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(audio_multinet_task, "MultiNet Task", 6 * 1024, NULL, 5, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create multinet task");

    return ESP_OK;
}
