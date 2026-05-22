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

#define BUFFSIZE 1024
#define OTA_URL "http://10.176.68.23:8000/xiaozhi_update.bin"
#define OTA_VERSION_URL "http://10.176.68.23:8000/xiaozhi_version.json"

void check_ota_rollback(void);

void start_xiaozhi_ota(void);

#endif /* __OTA_H__ */
