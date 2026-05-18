#include "com_status.h"

#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "COM_STATUS";
bool is_awake = false;

#define AWAKE_IDLE_TIMEOUT_US (30 * 1000 * 1000LL)

com_status_t com_status = START;
static int64_t awake_idle_deadline_us = 0;

static const char *com_status_str[] = {
    "START",
    "IDLE",
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

void com_set_awake(bool awake)
{
    is_awake = awake;
    awake_idle_deadline_us = awake ? esp_timer_get_time() + AWAKE_IDLE_TIMEOUT_US : 0;
}

void com_awake_timeout_check(void)
{
    if (!is_awake || awake_idle_deadline_us == 0 || com_status == SPEAKING)
    {
        return;
    }

    if (esp_timer_get_time() < awake_idle_deadline_us)
    {
        return;
    }

    ESP_LOGI(TAG, "awake idle timeout, set is_awake=false");
    com_set_awake(false);
}

/**
 * @brief 改变状态
 *
 */
void com_status_change(com_status_t status)
{
    com_status_t previous_status = com_status;

    ESP_LOGI(TAG, "status change: %s -> %s",
             com_status_to_str(previous_status),
             com_status_to_str(status));

    com_status = status;

}
