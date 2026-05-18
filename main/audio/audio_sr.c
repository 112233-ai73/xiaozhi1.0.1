#include "audio_sr.h"

static const char *TAG = "AUDIO_SR";

static model_iface_data_t *model_data = NULL;
static const esp_mn_iface_t *multinet = NULL;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static QueueHandle_t g_result_que = NULL;
static srmodel_list_t *models = NULL;
static RingbufHandle_t afe_rb_1 = NULL;
static RingbufHandle_t afe_rb_2 = NULL;
static volatile bool g_vad_speech = false;

#define SPEECH_END_TIMEOUT_MS 500
#define WAKE_WORKING_HOLD_MS 500
#define NO_SPEECH_IDLE_TIMEOUT_MS 30000
#define AFE_RINGBUF_FRAME_NUM 8

static TickType_t ms_to_ticks(uint32_t timeout_ms)
{
    return pdMS_TO_TICKS(timeout_ms);
}

static bool timeout_elapsed(TickType_t now, TickType_t start, uint32_t timeout_ms)
{
    return (now - start) >= ms_to_ticks(timeout_ms);
}

static void send_afe_data_to_multinet(esp_afe_sr_data_t *afe_data, afe_fetch_result_t *res)
{
    if (res->data == NULL || afe_rb_1 == NULL)
    {
        return;
    }

    size_t afe_data_len = afe_handle->get_fetch_chunksize(afe_data) * sizeof(int16_t);
    if (xRingbufferSend(afe_rb_1, res->data, afe_data_len, pdMS_TO_TICKS(20)) != pdTRUE)
    {
        ESP_LOGW(TAG, "send afe_rb_1 failed, len=%u", (unsigned int)afe_data_len);
    }
}

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
    TickType_t working_enter_tick = 0;
    TickType_t last_voice_tick = 0;
    TickType_t speech_end_tick = 0;
    bool listening_has_speech = false;

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

        if (audio_mp3_is_playing())
        {
            g_vad_speech = false;
            continue;
        }

        if (com_status != START) {
            esp_mn_state_t mn_state = ESP_MN_STATE_DETECTING;

            mn_state = multinet->detect(model_data, res->data);

            if (ESP_MN_STATE_DETECTING == mn_state) {
                continue;
            }

            if (ESP_MN_STATE_TIMEOUT == mn_state) {
                ESP_LOGW(TAG, "Time out");
                sr_result_t result = {
                    .state = mn_state,
                    .command_id = 0,
                };
                xQueueSend(g_result_que, &result, 10);
                multinet->clean(model_data);
                continue;
            }

            if (ESP_MN_STATE_DETECTED == mn_state) {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++) {
                    ESP_LOGI(TAG, "TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                            i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
                }

                int sr_command_id = mn_result->command_id[0];
                ESP_LOGI(TAG, "Deteted command : %d", sr_command_id);
                sr_result_t result = {
                    .state = mn_state,
                    .command_id = sr_command_id,
                };
                xQueueSend(g_result_que, &result, 10);
#if !SR_CONTINUE_DET
                multinet->clean(model_data);
#endif
                continue;
            }
            ESP_LOGE(TAG, "Exception unhandled");
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

    afe_config->wakenet_init = false;
    afe_config->wakenet_model_name = NULL;
    afe_config->aec_init = true;
    afe_config->vad_init = true;
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    afe_handle = esp_afe_handle_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_handle, ESP_FAIL, TAG, "Failed create AFE handle");

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_data, ESP_FAIL, TAG, "Failed create AFE data");

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

    ret_val = xTaskCreatePinnedToCore(sr_handler_task, "SR Handler Task", 4 * 1024, g_result_que, 1, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio handler task");

    return ESP_OK;
}

QueueHandle_t app_sr_get_result_queue(void)
{
    return g_result_que;
}
