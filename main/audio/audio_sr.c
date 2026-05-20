#include "audio_sr.h"
#include "com/com_debug.h"

//static const char *TAG = "AUDIO_SR";

static model_iface_data_t *model_data = NULL;
static const esp_mn_iface_t *multinet = NULL;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static QueueHandle_t g_result_que = NULL;
static srmodel_list_t *models = NULL;
static volatile bool g_vad_speech = false;

#define VAD_IDLE_TIMEOUT_MS 1000
#define MULTINET_TIMEOUT_MS 5760

static TickType_t ms_to_ticks(uint32_t timeout_ms)
{
    return pdMS_TO_TICKS(timeout_ms);
}

static bool timeout_elapsed(TickType_t now, TickType_t start, uint32_t timeout_ms)
{
    return (now - start) >= ms_to_ticks(timeout_ms);
}

static void clean_multinet(void)
{
    if (multinet != NULL && model_data != NULL)
    {
        multinet->clean(model_data);
    }
}

static void switch_to_idle(void)
{
    if (com_status != IDLE)
    {
        com_status_change(IDLE);
    }
    clean_multinet();
}

static void switch_to_listening(void)
{
    if (com_status != LISTENING)
    {
        MY_LOGI("voice detected, enter listening");
        clean_multinet();
        com_status_change(LISTENING);
    }
}

static void send_sr_result(esp_mn_state_t state, int command_id)
{
    sr_result_t result = {
        .state = state,
        .command_id = command_id,
    };

    xQueueSend(g_result_que, &result, 10);
}

// static void log_fetch_result(const afe_fetch_result_t *res)
// {
//     MY_LOGI("VAD: %d, sample: %d",
//              res->vad_state,
//              res->data != NULL ? res->data[0] : 0);
// }

static void configure_afe(afe_config_t *afe_config)
{
    afe_config->pcm_config.mic_num = 2;
    afe_config->pcm_config.total_ch_num = 2;
    afe_config->se_init = false;
    afe_config->aec_init = true;               
    afe_config->ns_init = true;
    afe_config->afe_ns_mode = AFE_NS_MODE_NET;
    afe_config->agc_compression_gain_db = 15;
    afe_config->agc_target_level_dbfs = 3;
    afe_config->afe_linear_gain = 10.0;
    afe_config->wakenet_init = false;
    afe_config->wakenet_model_name = NULL;
    afe_config->vad_init = true;
    afe_config->vad_mode = VAD_MODE_3;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
}

static esp_err_t load_multinet_model(void)
{
    char *mn_name = esp_srmodel_filter(models, ESP_MN_CHINESE, NULL);
    if (mn_name == NULL)
    {
        MY_LOGE("No multinet model found");
        return ESP_FAIL;
    }

    multinet = esp_mn_handle_from_name(mn_name);
    ESP_RETURN_ON_FALSE(NULL != multinet, ESP_FAIL, TAG, "Failed create multinet handle");

    model_data = multinet->create(mn_name, MULTINET_TIMEOUT_MS);
    ESP_RETURN_ON_FALSE(NULL != model_data, ESP_FAIL, TAG, "Failed create multinet data");
    MY_LOGI("load multinet:%s", mn_name);

    return ESP_OK;
}

static void load_speech_commands(void)
{
    esp_mn_commands_clear();
    for (size_t i = 0; i < command_word_count; i++)
    {
        esp_mn_commands_add((int)i, cmd_phoneme[i]);
    }

    esp_mn_commands_update();
    esp_mn_commands_print();
    multinet->print_active_speech_commands(model_data);
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
        MY_LOGE("malloc feed buffer failed");
        vTaskDelete(NULL);
        return;
    }

    while (true)
    {
        audio_read(feed_buff, feed_size);
        afe_handle->feed(afe_data, feed_buff);
    }
}

static bool handle_vad_state(const afe_fetch_result_t *res, TickType_t *last_voice_tick)
{
    TickType_t now = xTaskGetTickCount();
    bool vad_speech = (res->vad_state == VAD_SPEECH);
    g_vad_speech = vad_speech;

    if (vad_speech)
    {
        *last_voice_tick = now;
        if (com_status == IDLE)
        {
            switch_to_listening();
        }
        return com_status == LISTENING;
    }

    if (com_status == LISTENING && timeout_elapsed(now, *last_voice_tick, VAD_IDLE_TIMEOUT_MS))
    {
        MY_LOGI("voice idle timeout, back to idle");
        switch_to_idle();
        return false;
    }

    return com_status == LISTENING;
}

static void handle_multinet_detect(afe_fetch_result_t *res)
{
    esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

    if (ESP_MN_STATE_DETECTING == mn_state)
    {
        return;
    }

    if (ESP_MN_STATE_TIMEOUT == mn_state)
    {
        MY_LOGW("Time out");
        send_sr_result(mn_state, 0);
        switch_to_idle();
        return;
    }

    if (ESP_MN_STATE_DETECTED == mn_state)
    {
        esp_mn_results_t *mn_result = multinet->get_results(model_data);
        for (int i = 0; i < mn_result->num; i++)
        {
            MY_LOGI("TOP %d, command_id: %d, phrase_id: %d, prob: %f",
                     i + 1, mn_result->command_id[i], mn_result->phrase_id[i], mn_result->prob[i]);
        }

        int sr_command_id = mn_result->command_id[0];
        MY_LOGI("Deteted command : %d", sr_command_id);
        send_sr_result(mn_state, sr_command_id);
#if !SR_CONTINUE_DET
        clean_multinet();
#endif
        return;
    }

    MY_LOGE("Exception unhandled");
}

static void audio_detect_task(void *pvParam)
{
    com_status_change(IDLE);
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)pvParam;
    TickType_t last_voice_tick = 0;

    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);

    while (true)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            MY_LOGE("fetch error!");
            continue;
        }

        //log_fetch_result(res);
        com_awake_timeout_check();

        if (audio_mp3_is_playing())
        {
            g_vad_speech = false;
            if (com_status == LISTENING)
            {
                switch_to_idle();
            }
            continue;
        }

        if (handle_vad_state(res, &last_voice_tick))
        {
            handle_multinet_detect(res);
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
    configure_afe(afe_config);

    afe_handle = esp_afe_handle_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_handle, ESP_FAIL, TAG, "Failed create AFE handle");

    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    ESP_RETURN_ON_FALSE(NULL != afe_data, ESP_FAIL, TAG, "Failed create AFE data");

    ESP_RETURN_ON_ERROR(load_multinet_model(), TAG, "Failed load multinet model");
    load_speech_commands();

    BaseType_t ret_val = xTaskCreatePinnedToCore(audio_feed_task, "Feed Task", 4 * 1024, afe_data, 5, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create audio feed task");

    ret_val = xTaskCreatePinnedToCore(audio_detect_task, "Detect Task", 6 * 1024, afe_data, 5, NULL, 0);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create audio detect task");

    ret_val = xTaskCreatePinnedToCore(sr_handler_task, "SR Handler Task", 4 * 1024, g_result_que, 1, NULL, 1);
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_FAIL, TAG, "Failed create audio handler task");

    return ESP_OK;
}

QueueHandle_t app_sr_get_result_queue(void)
{
    return g_result_que;
}
