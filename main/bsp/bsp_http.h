#ifndef __BSP_HTTP_H__
#define __BSP_HTTP_H__

#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"

#include "bsp_mutable_buffer.h"
#include "bsp_esp32s3.h"
#include "cJSON.h"
#include "com_debug.h"

#include "esp_app_desc.h"

/**
 * @brief 发送http请求 请求激活码
 */
void http_request_active_code(void);

#endif /* __BSP_HTTP_H__ */
