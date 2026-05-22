#ifndef __AUDIO_MP3_DECODE_H__
#define __AUDIO_MP3_DECODE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "audio_sr.h"

#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"

#include "audio_init.h"

esp_err_t mount_storage_partition(void);
esp_err_t audio_mp3_play_file_async(const char *file_name);
bool audio_mp3_is_playing(void);
void stop_play_mp3(void);

void audio_mp3_decode_task(void);
void mp3_player_task(void *pvParameters);

#endif /* __AUDIO_MP3_DECODE_H__ */
