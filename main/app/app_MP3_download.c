#include "app_MP3_download.h"
#include "audio_mp3_decode.h"
#include "bsp/bsp_sdcard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <unistd.h>

#define MP3_DOWNLOAD_URL_MAX_LEN 160
#define MP3_DOWNLOAD_PATH_MAX_LEN 96

typedef struct
{
    char url[MP3_DOWNLOAD_URL_MAX_LEN];
    char file_path[MP3_DOWNLOAD_PATH_MAX_LEN];
} mp3_download_request_t;

static volatile bool s_mp3_downloading = false;

static bool is_sdcard_file_path(const char *file_path)
{
    return file_path != NULL && strncmp(file_path, "/sdcard/", strlen("/sdcard/")) == 0;
}

static void mp3_download_task(void *param)
{
    mp3_download_request_t *request = (mp3_download_request_t *)param;

    if (request == NULL)
    {
        s_mp3_downloading = false;
        vTaskDelete(NULL);
        return;
    }

    esp_err_t ret = mount_storage_partition();
    if (ret == ESP_OK)
    {
        ret = download_audio_to_vfs(request->url, request->file_path);
    }

    if (ret == ESP_OK)
    {
        MY_LOGI("MP3 download task finished: %s", request->file_path);
        if (is_sdcard_file_path(request->file_path))
        {
            esp_err_t list_ret = sd_list_files("/sdcard");
            if (list_ret != ESP_OK)
            {
                MY_LOGW("failed to refresh sd_file_names after download: %s", esp_err_to_name(list_ret));
            }
        }
    }
    else
    {
        MY_LOGE("MP3 download task failed: %s", esp_err_to_name(ret));
    }

    heap_caps_free(request);
    s_mp3_downloading = false;
    vTaskDelete(NULL);
}

esp_err_t app_mp3_download_start_async(const char *url, const char *file_path)
{
    if (url == NULL || url[0] == '\0' || file_path == NULL || file_path[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mp3_downloading)
    {
        MY_LOGW("MP3 download already running");
        return ESP_ERR_INVALID_STATE;
    }

    mp3_download_request_t *request = (mp3_download_request_t *)heap_caps_calloc(
        1, sizeof(mp3_download_request_t), MALLOC_CAP_DEFAULT);
    if (request == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    strlcpy(request->url, url, sizeof(request->url));
    strlcpy(request->file_path, file_path, sizeof(request->file_path));

    s_mp3_downloading = true;
    BaseType_t task_ret = xTaskCreate(mp3_download_task,
                                      "mp3_download",
                                      MP3_DOWNLOAD_TASK_STACK_SIZE,
                                      request,
                                      MP3_DOWNLOAD_TASK_PRIORITY,
                                      NULL);
    if (task_ret != pdPASS)
    {
        s_mp3_downloading = false;
        heap_caps_free(request);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t download_audio_to_vfs(const char *url, const char *file_path)
{
    esp_err_t ret = ESP_OK;
    FILE *f = NULL;
    char *psram_buf = NULL;
    char *network_buf = NULL;
    int psram_buf_offset = 0;

    psram_buf = (char *)heap_caps_malloc(PSRAM_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (psram_buf == NULL)
    {
        MY_LOGE("failed to allocate PSRAM buffer: %d bytes", PSRAM_BUFFER_SIZE);
        return ESP_ERR_NO_MEM;
    }

    network_buf = (char *)heap_caps_malloc(NETWORK_CHUNK_SIZE, MALLOC_CAP_DEFAULT);
    if (network_buf == NULL)
    {
        MY_LOGE("failed to allocate network buffer: %d bytes", NETWORK_CHUNK_SIZE);
        heap_caps_free(psram_buf);
        return ESP_ERR_NO_MEM;
    }

    MY_LOGI("download buffers ready, psram=%d network=%d", PSRAM_BUFFER_SIZE, NETWORK_CHUNK_SIZE);

    f = fopen(file_path, "wb");
    if (f == NULL)
    {
        MY_LOGE("failed to open file for write: %s", file_path);
        heap_caps_free(network_buf);
        heap_caps_free(psram_buf);
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        MY_LOGE("HTTP client init failed");
        fclose(f);
        heap_caps_free(network_buf);
        heap_caps_free(psram_buf);
        return ESP_FAIL;
    }

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK)
    {
        MY_LOGE("failed to connect server: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    int content_length = esp_http_client_fetch_headers(client);
    MY_LOGI("start download, content_length=%d", content_length);

    int total_downloaded = 0;
    while (1)
    {
        int read_len = esp_http_client_read_response(client, network_buf, NETWORK_CHUNK_SIZE);

        if (read_len < 0)
        {
            MY_LOGE("network read failed");
            ret = ESP_FAIL;
            break;
        }
        if (read_len == 0)
        {
            MY_LOGI("network stream download finished");
            break;
        }

        total_downloaded += read_len;

        if (psram_buf_offset + read_len > PSRAM_BUFFER_SIZE)
        {
            size_t written = fwrite(psram_buf, 1, psram_buf_offset, f);
            if (written != (size_t)psram_buf_offset)
            {
                MY_LOGE("file write failed, storage may be full");
                ret = ESP_FAIL;
                break;
            }
            psram_buf_offset = 0;
        }

        memcpy(psram_buf + psram_buf_offset, network_buf, read_len);
        psram_buf_offset += read_len;

        if (content_length > 0)
        {
            MY_LOGI("download progress: %.2f%% (%d/%d)",
                    (float)total_downloaded / content_length * 100.0f,
                    total_downloaded,
                    content_length);
        }
    }

    if (ret == ESP_OK && psram_buf_offset > 0)
    {
        size_t written = fwrite(psram_buf, 1, psram_buf_offset, f);
        if (written != (size_t)psram_buf_offset)
        {
            MY_LOGE("final file write failed");
            ret = ESP_FAIL;
        }
    }

cleanup:
    if (client != NULL)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (f != NULL)
    {
        fclose(f);
    }
    if (network_buf != NULL)
    {
        heap_caps_free(network_buf);
    }
    if (psram_buf != NULL)
    {
        heap_caps_free(psram_buf);
    }

    if (ret == ESP_OK)
    {
        MY_LOGI("audio downloaded to: %s", file_path);
    }
    else
    {
        MY_LOGE("audio download failed");
        unlink(file_path);
    }

    return ret;
}
