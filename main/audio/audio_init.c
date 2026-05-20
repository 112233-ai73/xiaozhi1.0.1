#include "audio_init.h"
#include "com/com_debug.h"

static const char *TAG = "AUDIO_INIT";

// 静态句柄
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static esp_codec_dev_handle_t play_dev_handle = NULL;
static esp_codec_dev_handle_t record_dev_handle = NULL;

/**
 * 第一步：初始化 I2C 总线
 */
static esp_err_t audio_i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    return i2c_new_master_bus(&i2c_bus_conf, &i2c_bus_handle);
}

/**
 * 第二步：初始化 I2S
 * 配置为全双工：TX 使用标准模式(ES8311)，RX 使用 TDM 模式(ES7210)
 */
static esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear = true
    };
    // 1. 创建句柄
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    // 2. 统一使用 TDM 配置 (解决模式冲突)
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(16000), 
        .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din = AUDIO_I2S_GPIO_DIN,
        }
    };
    
    // TX 和 RX 都使用同样的 TDM 初始化逻辑以保持端口状态一致
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(tx_handle, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle, &tdm_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    return ESP_OK;
}

esp_codec_dev_handle_t audio_get_playback_handle(void)
{
    return play_dev_handle;
}
/**
 * 第三步：初始化 ES8311 (播放设备)
 */
static esp_err_t audio_es8311_init(void)
{
    // 1. 控制接口 (I2C)
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = AUDIO_CODEC_ES8311_ADDR,
        .bus_handle = i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    // 2. 数据接口 (I2S)
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    // 3. 实例化驱动
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = out_ctrl_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = AUDIO_CODEC_PA_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);

    // 4. 创建播放设备句柄
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if,
        .data_if = data_if,
    };
    play_dev_handle = esp_codec_dev_new(&dev_cfg);
    // 设置格式并启动设备
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = 16000};
    ESP_ERROR_CHECK(esp_codec_dev_open(play_dev_handle, &fs));
    // 设置初始音量为 60dB
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(play_dev_handle, 50));

    return (play_dev_handle != NULL) ? ESP_OK : ESP_FAIL;
}

/**
 * 第四步：初始化 ES7210 (录音设备)
 */
static esp_err_t audio_es7210_init(void)
{
    // 1. 控制接口 (I2C)
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = AUDIO_CODEC_ES7210_ADDR,
        .bus_handle = i2c_bus_handle,
    };
    const audio_codec_ctrl_if_t *in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    // 2. 实例化驱动
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .mic_selected = ES7210_SEL_MIC1|ES7210_SEL_MIC2, // 根据原理图选择开启的麦克风
    };
    const audio_codec_if_t *in_codec_if = es7210_codec_new(&es7210_cfg);

    // 3. 数据接口复用前面的 i2s_data 接口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    // 4. 创建录音设备句柄
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = in_codec_if,
        .data_if = data_if,
    };
    record_dev_handle = esp_codec_dev_new(&dev_cfg);
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = 16000};
    ESP_ERROR_CHECK(esp_codec_dev_open(record_dev_handle, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(record_dev_handle, 30.0));

    return (record_dev_handle != NULL) ? ESP_OK : ESP_FAIL;
}

/**
 * 汇总初始化入口
 */
esp_err_t audio_init(void)
{
    MY_LOGI("Initializing Audio Hardware...");

    ESP_ERROR_CHECK(audio_i2c_init());
    MY_LOGD("Step 1: I2C Initialized");

    ESP_ERROR_CHECK(audio_i2s_init());
    MY_LOGD("Step 2: I2S Initialized");

    ESP_ERROR_CHECK(audio_es8311_init());
    MY_LOGD("Step 3: ES8311 Initialized");

    ESP_ERROR_CHECK(audio_es7210_init());
    MY_LOGD("Step 4: ES7210 Initialized");

    MY_LOGI("Audio System Started Successfully.");
// 在 audio_init 函数末尾添加
gpio_config_t pa_conf = {
    .pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&pa_conf);
gpio_set_level(AUDIO_CODEC_PA_PIN, 1);

    return ESP_OK;
}

/**
 * @brief 从麦克风读取数据
 *
 */
int audio_read(void *dest, int size)
{
    if (record_dev_handle == NULL)
        return 0;
    // esp_codec_dev_read 返回 ESP_OK 代表读取成功
    esp_err_t ret = esp_codec_dev_read(record_dev_handle, dest, size);
    return (ret == ESP_OK) ? size : 0;
}

/**
 * 向扬声器写入数据
 */
int audio_write(const void *src, int size)
{
    if (play_dev_handle == NULL)
        return 0;
    // esp_codec_dev_write 将数据写入 I2S 缓冲区
    esp_err_t ret = esp_codec_dev_write(play_dev_handle, (void *)src, size);
    return (ret == ESP_OK) ? size : 0;
}

/**
 * @brief es8311 修改音量大小
 */
void inf_es8311_set_volume(int volume)
{
    esp_codec_dev_set_out_vol(play_dev_handle, volume);
}

/**
 * @brief es8311 静音状态设置
 */
void inf_es8311_set_mute(bool mute)
{
    esp_codec_dev_set_out_mute(play_dev_handle, mute);
}
