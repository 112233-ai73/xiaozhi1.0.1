#include "audio_init.h"

i2s_chan_handle_t mic_handle;
i2s_chan_handle_t speaker_handle;

esp_codec_dev_handle_t codec_dev_out;
esp_codec_dev_handle_t codec_dev_in;

static void i2c_init(void)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
    };

    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, i2c_cfg.mode, 0, 0, 0);
}

static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din = AUDIO_I2S_GPIO_DIN,
        },
    };

    i2s_new_channel(&chan_cfg, &speaker_handle, &mic_handle);
    i2s_channel_init_std_mode(speaker_handle, &std_cfg);
    i2s_channel_init_std_mode(mic_handle, &std_cfg);
    i2s_channel_enable(speaker_handle);
    i2s_channel_enable(mic_handle);
}

static void audio_es8311_init(void)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .tx_handle = speaker_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg_out = {
        .addr = AUDIO_CODEC_ES8311_ADDR,
    };
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg_out);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = AUDIO_CODEC_PA_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);

    esp_codec_dev_cfg_t dev_cfg_out = {
        .codec_if = out_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
    };
    codec_dev_out = esp_codec_dev_new(&dev_cfg_out);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(codec_dev_out, &fs);
    esp_codec_dev_set_out_vol(codec_dev_out, 60);
}

static void audio_es7210_init(void)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = mic_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg_in = {
        .addr = AUDIO_CODEC_ES7210_ADDR,
    };
    const audio_codec_ctrl_if_t *in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg_in);

    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .master_mode = false,
        .mic_selected = ES7210_SEL_MIC1,
        .mclk_div = 256,
    };
    const audio_codec_if_t *in_codec_if = es7210_codec_new(&es7210_cfg);

    esp_codec_dev_cfg_t dev_cfg_in = {
        .codec_if = in_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
    };
    codec_dev_in = esp_codec_dev_new(&dev_cfg_in);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(codec_dev_in, &fs);
    esp_codec_dev_set_in_gain(codec_dev_in, 20.0f);
}

void audio_init(void)
{
    i2c_init();
    i2s_init();
    audio_es8311_init();
    audio_es7210_init();
}

void audio_es7210_read_mic(uint8_t *buf, int len)
{
    if (codec_dev_in != NULL)
    {
        esp_codec_dev_read(codec_dev_in, (void *)buf, len);
    }
}

void audio_es8311_write_speaker(uint8_t *buf, int len)
{
    if (codec_dev_out != NULL)
    {
        esp_codec_dev_write(codec_dev_out, (void *)buf, len);
    }
}

/**
 * @brief es8311 修改音量大小
 */
void inf_es8311_set_volume(int volume)
{
    esp_codec_dev_set_out_vol(codec_dev_out, volume);
}

/**
 * @brief es8311 静音状态设置
 */
void inf_es8311_set_mute(bool mute)
{
    esp_codec_dev_set_out_mute(codec_dev_out, mute);
}

