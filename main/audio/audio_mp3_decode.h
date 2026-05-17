#ifndef __AUDIO_MP3_DECODE_H__
#define __AUDIO_MP3_DECODE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"

#include "audio_init.h"

esp_err_t mount_storage_partition(void);
esp_err_t audio_mp3_play_file_async(const char *file_name);

void audio_mp3_decode_task(void);
void mp3_player_task(void *pvParameters);

#endif /* __AUDIO_MP3_DECODE_H__ */
