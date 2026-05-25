#ifndef __APP_OFFLINE_SR_HANDLER_H__
#define __APP_OFFLINE_SR_HANDLER_H__

#include <stdbool.h>
#include "esp_err.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "audio_sr.h"
#include "esp_afe_sr_iface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "bsp/bsp_usart.h"
#include "com/com_status.h"

void sr_handler_task(void *pvParam);

#endif /* __APP_OFFLINE_SR_HANDLER_H__ */
