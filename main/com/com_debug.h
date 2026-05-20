#ifndef __COM_DEBUG_H__
#define __COM_DEBUG_H__

#include "esp_log.h"
#include "stdio.h"
#include "string.h"
#include "esp_task.h"

// 选择开发环境和生产环境
#define DEBUG 1

extern char TAG[100];
#if (DEBUG == 1)

#define MY_LOGE(format, ...)                                \
    do                                                      \
    {                                                       \
        sprintf(TAG, "[%10s:%4d]", __FILENAME__, __LINE__); \
        ESP_LOGE(TAG, format, ##__VA_ARGS__);               \
    } while (0)
#define MY_LOGW(format, ...)                                \
    do                                                      \
    {                                                       \
        sprintf(TAG, "[%10s:%4d]", __FILENAME__, __LINE__); \
        ESP_LOGW(TAG, format, ##__VA_ARGS__);               \
    } while (0)

#define MY_LOGI(format, ...)                                \
    do                                                      \
    {                                                       \
        sprintf(TAG, "[%10s:%4d]", __FILENAME__, __LINE__); \
        ESP_LOGI(TAG, format, ##__VA_ARGS__);               \
    } while (0)

#define MY_LOGD(format, ...)                                \
    do                                                      \
    {                                                       \
        sprintf(TAG, "[%10s:%4d]", __FILENAME__, __LINE__); \
        ESP_LOGD(TAG, format, ##__VA_ARGS__);               \
    } while (0)

#define MY_LOGV(format, ...)                                \
    do                                                      \
    {                                                       \
        sprintf(TAG, "[%10s:%4d]", __FILENAME__, __LINE__); \
        ESP_LOGV(TAG, format, ##__VA_ARGS__);               \
    } while (0)
#else
#define MY_LOGE(format, ...)
#define MY_LOGW(format, ...)
#define MY_LOGI(format, ...)
#define MY_LOGD(format, ...)
#define MY_LOGV(format, ...)
#endif

#endif /* __COM_DEBUG_H__ */
