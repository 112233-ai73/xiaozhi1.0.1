#include "com_status.h"

#include "esp_log.h"

static const char *TAG = "COM_STATUS";

com_status_t com_status = START;

static const char *com_status_str[] = {
    "START",
    "IDLE",
    "WORKING",
    "LISTENING",
    "SPEAKING",
};

static const char *com_status_to_str(com_status_t status)
{
    if (status >= START && status <= SPEAKING)
    {
        return com_status_str[status];
    }

    return "UNKNOWN";
}

/**
 * @brief 改变状态
 *
 */
void com_status_change(com_status_t status)
{
    ESP_LOGI(TAG, "status change: %s -> %s",
             com_status_to_str(com_status),
             com_status_to_str(status));

    com_status = status;
}
