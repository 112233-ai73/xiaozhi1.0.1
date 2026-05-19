#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "audio/audio_init.h"
#include "audio/audio_mp3_decode.h"
#include "audio/audio_sr.h"
#include "usart/usart_init.h"
#include "bsp/bsp_wifi.h"


static const char *TAG = "SR_TEST";

#define STARTUP_MP3_FILE "107.mp3"
#define STARTUP_MP3_WAIT_MS 7500

void app_main(void)
{
    ESP_LOGI(TAG, "speech recognition test start");

    usart_init();
    ESP_ERROR_CHECK(audio_init());
    // bsp_wifi_init();
    esp_err_t ret = audio_mp3_play_file_async(STARTUP_MP3_FILE);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "startup MP3 playback failed: %s", esp_err_to_name(ret));
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(STARTUP_MP3_WAIT_MS));
    }

    ESP_ERROR_CHECK(app_sr_start());
}
