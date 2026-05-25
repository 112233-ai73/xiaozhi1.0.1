#ifndef __AUDIO_DECODE_H__
#define __AUDIO_DECODE_H__

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "unity.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_heap_caps.h"

typedef struct audio_decode audio_decode_t;

/**
 * @brief 创建编码器结构体实例
 */
audio_decode_t *audio_decode_create();

/**
 * @brief 启动解码器
 */
void audio_decode_start(audio_decode_t *audio_decode);

/**
 * @brief 注册输入缓冲区函数
 */
void audio_decode_register_input_ringbuf(audio_decode_t *audio_decode, RingbufHandle_t ringbuf_in);

/**
 * @brief 注册输出缓冲区函数
 */
void audio_decode_register_output_ringbuf(audio_decode_t *audio_decode, RingbufHandle_t ringbuf_out);

#endif /* __AUDIO_DECODE_H__ */
