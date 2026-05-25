#ifndef __BSP_WS_H__
#define __BSP_WS_H__
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include <cJSON.h>
#include "bsp_esp32s3.h"
#include "com_debug.h"
#include "audio_online_sr_handle.h"
#include "com_status.h"

typedef void (*text_callback_t)(char *data, int len);
typedef void (*bin_callback_t)(char *data, int len);

extern char session_id[9];

extern EventGroupHandle_t ws_event_group;
extern int WEBSOCKET_SESSION_ID_BIT;
/**
 * @brief 初始化WS
 */
void bsp_ws_init(text_callback_t text_cb, bin_callback_t bin_cb);

/**
 * @brief WS开启
 */
void bsp_ws_start(void);

/**
 * @brief WS关闭
 */
void bsp_ws_stop(void);

/**
 * @brief 发送hello消息
 */
void bsp_ws_send_hello(void);

/**
 * @brief 发送唤醒词
 * 我们口头唤醒的时候,只是唤醒了sr组件,但是我们在说唤醒词的时候,sr组件并没有把我们的音频发送出去
 * 我们需要再口头唤醒的时候,使用ws发送一个唤醒词的请求给服务器 同步唤醒
 */
void bsp_ws_send_wakeup_word(void);

/**
 * @brief 发送开始监听请求
 */
void bsp_ws_send_start_listen(void);

/**
 * @brief 发送结束监听请求
 */
void bsp_ws_send_stop_listen(void);

/**
 * @brief 发送中断请求
 */
void bsp_ws_send_abort(void);

/**
 * @brief 发送opus数据
 */
void bsp_ws_send_opus(char *data, int len);

/**
 * @brief 发送IOT控制学习
 */
void bsp_ws_send_iot_learn(void);

/**
 * @brief 发送IOT状态
 */
void bsp_ws_send_iot_status(void);
#endif /* __BSP_WS_H__ */
