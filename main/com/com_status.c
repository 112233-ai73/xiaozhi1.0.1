#include "com_status.h"

#include "audio/audio_mp3_decode.h"
#include "esp_log.h"

static const char *TAG = "COM_STATUS";
bool is_awake = false;

#define WORKING_TO_IDLE_MP3_FILE "106.mp3"

com_status_t com_status = START;

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

    if ( status == IDLE)
    {
        audio_mp3_play_file_async(WORKING_TO_IDLE_MP3_FILE);
    }
}
