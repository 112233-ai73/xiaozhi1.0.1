#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"


#include "audio/audio_init.h"
#include "audio/audio_sr.h"
#include "com/com_status.h"
#include "com/command_word.h"

static const char *TAG = "SR_TEST";

static const char *sr_test_command_name(int command_id)
{
    if (command_id >= 0 && command_id < (int)command_word_count)
    {
        return cmd_phoneme[command_id];
    }

    return "unknown";
}

static void sr_result_test_task(void *arg)
{
    QueueHandle_t result_queue = (QueueHandle_t)arg;
    sr_result_t result = {0};

    while (true)
    {
        if (xQueueReceive(result_queue, &result, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (result.wakenet_mode == WAKENET_DETECTED)
        {
            ESP_LOGI(TAG, "wakeword detected");
            continue;
        }

        if (result.state == ESP_MN_STATE_DETECTED)
        {
            ESP_LOGI(TAG, "command detected: id=%d, name=%s, confidence=%.3f",
                     result.command_id,
                     sr_test_command_name(result.command_id),
                     result.confidence);
        }
    }
}

static void sr_listening_test_task(void *arg)
{
    (void)arg;

    while (true)
    {
        if (com_status == WORKING)
        {
            ESP_LOGI(TAG, "enter command listening");
            com_status_change(LISTENING);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "speech recognition test start");

    ESP_ERROR_CHECK(audio_init());
    ESP_ERROR_CHECK(app_sr_start());

    QueueHandle_t result_queue = app_sr_get_result_queue();
    if (result_queue != NULL)
    {
        xTaskCreate(sr_result_test_task, "sr_result_test", 4 * 1024, result_queue, 5, NULL);
    }

    xTaskCreate(sr_listening_test_task, "sr_listening_test", 3 * 1024, NULL, 5, NULL);
}
