#ifndef __OTA_H__
#define __OTA_H__

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

#define BUFFSIZE 1024
#define OTA_URL "http://192.168.26.3:8000/xiaozhi_update.bin"

#endif /* __OTA_H__ */
