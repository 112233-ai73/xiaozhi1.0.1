#ifndef __OTA_H__
#define __OTA_H__

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "com/com_debug.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "cJSON.h"

#define BUFFSIZE 1024
#define OTA_VERSION_URL "http://192.168.26.2:8080/xiaozhi_update.json"
#define OTA_JSON_BUFFER_SIZE 512
#define OTA_URL_MAX_LEN 256

void check_ota_rollback(void);

void start_xiaozhi_ota(void);

#endif /* __OTA_H__ */
