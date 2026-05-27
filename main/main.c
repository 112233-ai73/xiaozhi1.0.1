#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "com/com_debug.h"

#include "audio/audio_init.h"
#include "audio/audio_sr.h"
#include "audio/audio_mp3_decode.h"
#include "audio/audio_online_sr_handle.h"
#include "bsp/bsp_usart.h"
#include "bsp/bsp_sdcard.h"
#include "bsp/bsp_wifi.h"
#include "bsp/bsp_http.h"
#include "bsp/bsp_nvs.h"
#include "bsp/bsp_ws.h"
#include "app/app_MP3_download.h"
#include "ota.h"

#define STARTUP_MP3_FILE "/spiffs/107.mp3"
#define HTTP_WIFI_WAIT_MS 15000

bool is_yueguang = false;

static void play_startup_prompt(void)
{
    esp_err_t ret = audio_mp3_play_file_async_without_multinet(STARTUP_MP3_FILE);
    if (ret != ESP_OK)
    {
        MY_LOGW("startup MP3 playback failed: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    MY_LOGI("speech recognition test start");
    bsp_wifi_init();
    check_ota_rollback();
    init_sd_card_spi();
    ESP_ERROR_CHECK(sd_list_files("/sdcard"));
    usart_init();
    ESP_ERROR_CHECK(audio_init());
    
    ESP_ERROR_CHECK(app_sr_start());
    ESP_ERROR_CHECK(audio_online_init());

    play_startup_prompt();

    // esp_err_t wifi_ret = bsp_wifi_wait_connected(pdMS_TO_TICKS(HTTP_WIFI_WAIT_MS));
    // if (wifi_ret == ESP_OK)
    // {  
    //     http_request_active_code();
    //     bsp_ws_start();
    // }
    // else
    // {
    //     MY_LOGW("skip active code request, WiFi not ready: %s", esp_err_to_name(wifi_ret));
    // }
    
}
