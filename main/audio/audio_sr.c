#include "audio_sr.h"

static const char *TAG = "AUDIO_SR";

static model_iface_data_t *model_data = NULL;
static const esp_mn_iface_t *multinet = NULL;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static QueueHandle_t g_result_que = NULL;
static srmodel_list_t *models = NULL;
static RingbufHandle_t afe_rb_1 = NULL;
static RingbufHandle_t afe_rb_2 = NULL;

static void audio_sr_feed_task(void *arg)
{
    // 任务功能,持续去es8311中读取mic数据,并喂给afe_data

    // 先拿到arg参数
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;

    // 3.1 根据afe_data实例中配置的采样率 和 内部人家自己规定的 数据时长 计算每次处理的数据大小的一个计算值
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // 3.2 获取afe_data是实例中配置的通道数
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    // 3.3 根据通道数 位深 和 上边根据数据时长采样率 这几个数据  得到每次处理数据的大小
    size_t feed_size = feed_chunksize * feed_nch * sizeof(int16_t);
    // 3.4 根据每次要处理的数据大小 开辟一个缓冲区
    int16_t *feed_buff = (int16_t *)malloc(feed_size);

    // 3.5 启动喂养
    while (1)
    {
        // 3.6 从es8311中读取pcm数据 放到feed_buff的缓冲区中
        // 3.7 es8311 读取数据是阻塞的,只有读到对应传入的长度的数据 才能解除阻塞
        audio_read(feed_buff, feed_size);

        // 3.8 feed_buff缓冲区 拿到了 feed_size 长度的数据 交给afe_data去处理
        afe_handle->feed(afe_data, feed_buff);
    }
}

static void audio_detect_task(void *pvParam)
{
    com_status_change(IDLE);
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)pvParam;

    /* Check if audio data has same chunksize with multinet */
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
            ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_GREEN) "Wakeword detected");
            sr_result_t result = {
                .wakenet_mode = WAKENET_DETECTED,
                .state = ESP_MN_STATE_DETECTING,
                .command_id = 0,
            };
            xQueueSend(g_result_que, &result, 10);
            continue;
        }
        else if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            ESP_LOGI(TAG, LOG_BOLD(LOG_COLOR_GREEN) "Channel verified");
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
                afe_rb_1 = xRingbufferCreate(feed_size, RINGBUF_TYPE_BYTEBUF);
                if (afe_rb_1 == NULL)
                {
                    ESP_LOGE(TAG, "create afe_rb_1 failed");
                    continue;
                }
            }

            if (afe_rb_2 == NULL)
            {
                afe_rb_2 = xRingbufferCreate(feed_size, RINGBUF_TYPE_BYTEBUF);
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

esp_err_t app_sr_start(void)
{
    g_result_que = xQueueCreate(1, sizeof(sr_result_t));
    ESP_RETURN_ON_FALSE(NULL != g_result_que, ESP_ERR_NO_MEM, TAG, "Failed create result queue");

    models = esp_srmodel_init("model");

    afe_handle = &ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config.aec_init = false;

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);
    ESP_LOGI(TAG, "load wakenet:%s", afe_config.wakenet_model_name);

    // for (int i = 0; i < models->num; i++) {
    //     ESP_LOGI(TAG, "Current Model:%s", models->model_name[i]);
    // }

    char *mn_name = esp_srmodel_filter(models, ESP_MN_CHINESE, NULL);
    if (NULL == mn_name) {
        ESP_LOGE(TAG, "No multinet model found");
        return ESP_FAIL;
    }

    multinet = esp_mn_handle_from_name(mn_name);
    model_data = multinet->create(mn_name, 5760);
    ESP_LOGI(TAG, "load multinet:%s", mn_name);

    esp_mn_commands_clear();

    for (int i = 0; i < sizeof(cmd_phoneme) / sizeof(cmd_phoneme[0]); i++) {
        esp_mn_commands_add(i, (char *)cmd_phoneme[i]);
    }

    esp_mn_commands_update();
    esp_mn_commands_print();
    multinet->print_active_speech_commands(model_data);

    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "Feed Task", 4 * 1024, afe_data, 5, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "Detect Task", 6 * 1024, afe_data, 5, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(sr_handler_task, "SR Handler Task", 4 * 1024, g_result_que, 1, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG,  "Failed create audio handler task");

    return ESP_OK;
}
