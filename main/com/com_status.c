#include "com_status.h"
#include "com/com_debug.h"

#include "audio/audio_mp3_decode.h"
#include "esp_log.h"
#include "esp_timer.h"

#define AWAKE_TIMEOUT_US       (30 * 1000 * 1000LL)
#define SLEEP_PROMPT_MP3_FILE  "106.mp3"

//static const char *TAG = "COM_STATUS";

com_status_t com_status = START;
bool is_awake = false;
bool MP3_after_awake = false;

static int64_t s_awake_deadline_us = 0;

static const char *com_status_to_str(com_status_t status)
{
    switch (status) {
    case START:
        return "START";
    case IDLE:
        return "IDLE";
    case LISTENING:
        return "LISTENING";
    case SPEAKING:
        return "SPEAKING";
    default:
        return "UNKNOWN";
    }
}

static void com_awake_timer_start(void)
{
    s_awake_deadline_us = esp_timer_get_time() + AWAKE_TIMEOUT_US;
}

static void com_awake_timer_stop(void)
{
    s_awake_deadline_us = 0;
}

void com_set_awake(bool awake)
{
    if (is_awake == awake) {
        if (awake && com_status != SPEAKING) {
            com_awake_timer_start();
        }
        return;
    }

    is_awake = awake;
    MY_LOGI("is_awake=%s", is_awake ? "true" : "false");

    if (is_awake && com_status != SPEAKING) {
        com_awake_timer_start();
    } else {
        com_awake_timer_stop();
    }
}

void com_status_change(com_status_t status)
{
    com_status_t previous_status = com_status;

    if (previous_status == status) {
        return;
    }

    com_status = status;
    MY_LOGI("status change: %s -> %s",
             com_status_to_str(previous_status),
             com_status_to_str(status));

    if (!is_awake) {
        return;
    }

    if (status == SPEAKING) {
        com_awake_timer_stop();
    } else if (previous_status == SPEAKING) {
        com_awake_timer_start();
    }
}

void com_awake_timeout_check(void)
{
    if (!is_awake || s_awake_deadline_us == 0 || com_status == SPEAKING) {
        return;
    }

    if (esp_timer_get_time() < s_awake_deadline_us) {
        return;
    }

    MY_LOGI("awake timeout, set is_awake=false");
    if (com_status == LISTENING) {
        com_status_change(IDLE);
    }
    com_set_awake(false);
    audio_mp3_play_file_async(SLEEP_PROMPT_MP3_FILE);
}
