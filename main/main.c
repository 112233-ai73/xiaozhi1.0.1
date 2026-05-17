#include "esp_err.h"
#include "esp_log.h"

#include "audio/audio_init.h"
#include "audio/audio_mp3_decode.h"
#include "audio/audio_sr.h"
#include "usart/usart_init.h"

static const char *TAG = "SR_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "speech recognition test start");

    usart_init();
    ESP_ERROR_CHECK(audio_init());
    ESP_ERROR_CHECK(app_sr_start());

    esp_err_t ret = audio_mp3_play_file_async("107.mp3");
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "startup MP3 playback failed: %s", esp_err_to_name(ret));
    }
}
