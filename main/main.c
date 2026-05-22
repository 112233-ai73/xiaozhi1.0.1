#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "com/com_debug.h"

#include "audio/audio_init.h"
#include "audio/audio_sr.h"
#include "audio/audio_mp3_decode.h"
#include "audio/audio_sr.h"
#include "bsp/bsp_usart.h"
#include "bsp/bsp_wifi.h"
#include "ota.h"

#define STARTUP_MP3_FILE "107.mp3"

static void play_startup_prompt(void)
{
    esp_err_t ret = audio_mp3_play_file_async(STARTUP_MP3_FILE);
    if (ret != ESP_OK)
    {
        MY_LOGW("startup MP3 playback failed: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    MY_LOGI("speech recognition test start");
    //bsp_wifi_init();
    //check_ota_rollback();
    usart_init();
    ESP_ERROR_CHECK(audio_init());
    
    ESP_ERROR_CHECK(app_sr_start());
    play_startup_prompt();
    
}
