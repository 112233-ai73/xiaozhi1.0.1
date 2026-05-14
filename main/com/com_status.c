#include "com_status.h"

#include "esp_log.h"

static const char *TAG = "COM_STATUS";

com_status_t com_status = START;

char *com_status_str[] = {
    "START",
    "IDLE",
    "WORKING",
    "LISTERENING",
    "SPEAKING",
};

/**
 * @brief 改变状态
 *
 */
void com_status_change(com_status_t status)
{
    ESP_LOGI(TAG, "status change: %s -> %s",com_status,status);

    com_status = status;
}
