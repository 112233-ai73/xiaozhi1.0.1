#ifndef __AUDIO_INIT_H__
#define __AUDIO_INIT_H__

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"


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
esp_err_t audio_init(void);
esp_codec_dev_handle_t audio_get_playback_handle(void);

/**
 * 从麦克风读取数据
 * @param buffer 数据缓冲区
 * @param size   期望读取的字节数
 * @return 实际读取的字节数
 */
int audio_read(void *dest, int size);

/**
 * 向扬声器写入数据
 * @param buffer 数据缓冲区
 * @param size   要写入的字节数
 * @return 实际写入的字节数
 */
int audio_write(const void *src, int size);

/**
 * @brief es8311 修改音量大小
 */
void inf_es8311_set_volume(int volume);

/**
 * @brief es8311 静音状态设置
 */
void inf_es8311_set_mute(bool mute);

#endif /* __AUDIO_INIT_H__ */
