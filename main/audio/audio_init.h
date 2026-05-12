#ifndef __AUDIO_INIT_H__
#define __AUDIO_INIT_H__

#include "common_debug.h"

#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "unity.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_40
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_21
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_48

#define AUDIO_CODEC_PA_PIN GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_38
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_47
#define AUDIO_CODEC_ES8311_ADDR (0x30)
#define AUDIO_CODEC_ES7210_ADDR (0x80)

#define AUDIO_CODEC_PA_PIN GPIO_NUM_46

/**
 * @brief 初始化音频系统
 * 
 */
void audio_init(void);

/**
 * @brief 读取ES7210麦克风数据
 * 
 * @param buf 缓冲区
 * @param len 数据长度
 */
void audio_es7210_read_mic(uint8_t *buf, int len);

/**
 * @brief 写入ES8311扬声器数据
 * 
 * @param buf 缓冲区
 * @param len 数据长度
 */
void audio_es8311_write_speaker(uint8_t *buf, int len);

/**
 * @brief es8311 修改音量大小
 */
void inf_es8311_set_volume(int volume);

/**
 * @brief es8311 静音状态设置
 */
void inf_es8311_set_mute(bool mute);

#endif /* __AUDIO_INIT_H__ */
