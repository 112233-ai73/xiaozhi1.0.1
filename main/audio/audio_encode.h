#ifndef __AUDIO_ENCODE_H__
#define __AUDIO_ENCODE_H__

#include "freertos/ringbuf.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_enc_reg.h"
#include "esp_audio_enc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct audio_encode audio_encode_t;

/**
 * @brief 创建一个编码器结构体实例
 */
audio_encode_t *audio_encode_create();

/**
 * @brief 启动编码器
 */
void audio_encode_start(audio_encode_t *audio_encode);

/**
 * @brief 给编码器注册输入缓冲区
 */
void audio_encode_register_input_ringbuf(audio_encode_t *audio_encode, RingbufHandle_t ringbuf_in);

/**
 * @brief 给编码器注册输出缓冲区
 */
void audio_encode_register_output_ringbuf(audio_encode_t *audio_encode, RingbufHandle_t ringbuf_out);

#endif /* __AUDIO_ENCODE_H__ */
