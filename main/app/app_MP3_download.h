#ifndef __APP_MP3_DOWNLOAD_H__
#define __APP_MP3_DOWNLOAD_H__

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "com/com_debug.h"

#define MP3_DOWNLOAD_TASK_STACK_SIZE (8 * 1024)
#define MP3_DOWNLOAD_TASK_PRIORITY   4
#define PSRAM_BUFFER_SIZE            (64 * 1024)
#define NETWORK_CHUNK_SIZE           (2048)

esp_err_t download_audio_to_vfs(const char *url, const char *file_path);
esp_err_t app_mp3_download_start_async(const char *url, const char *file_path);

#endif /* __APP_MP3_DOWNLOAD_H__ */
