#include "audio_mp3_decode.h"
#include "com/com_status.h"

static const char *TAG = "AUDIO_MP3_DECODE";

#define MP3_IN_BUF_SIZE  (2048)
#define PCM_OUT_BUF_SIZE (4096)
#define MP3_BASE_PATH    "/spiffs"
#define MP3_PARTITION    "mp3"
#define DEFAULT_MP3_FILE "test.mp3"

static SemaphoreHandle_t s_playback_mutex = NULL;
static volatile bool s_mp3_playing = false;

bool audio_mp3_is_playing(void)
{
    return s_mp3_playing;
}

esp_err_t mount_storage_partition(void)
{
    ESP_LOGI(TAG, "mounting SPIFFS partition...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = MP3_BASE_PATH,
        .partition_label = MP3_PARTITION,
        .max_files = 5,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPIFFS already mounted");
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "failed to find SPIFFS partition: %s", MP3_PARTITION);
        } else {
            ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(MP3_PARTITION, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS size: total=%u KB, used=%u KB",
                 (unsigned int)(total / 1024),
                 (unsigned int)(used / 1024));
    }

    return ESP_OK;
}

static esp_audio_err_t mp3_decode_file(FILE *file)
{
    esp_audio_err_t ret = esp_audio_dec_register_default();
    if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_ALREADY_EXIST) {
        ESP_LOGE(TAG, "register audio decoders failed: %d", ret);
        return ret;
    }

    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_MP3,
        .cfg = NULL,
        .cfg_sz = 0,
    };

    esp_audio_dec_handle_t decoder = NULL;
    ret = esp_audio_dec_open(&dec_cfg, &decoder);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "open MP3 decoder failed: %d", ret);
        return ret;
    }

    uint8_t *in_buf = (uint8_t *)malloc(MP3_IN_BUF_SIZE);
    uint8_t *out_buf = (uint8_t *)malloc(PCM_OUT_BUF_SIZE);
    uint32_t out_buf_size = PCM_OUT_BUF_SIZE;

    if (in_buf == NULL || out_buf == NULL) {
        ESP_LOGE(TAG, "malloc MP3 buffers failed");
        free(in_buf);
        free(out_buf);
        esp_audio_dec_close(decoder);
        return ESP_AUDIO_ERR_MEM_LACK;
    }

    bool audio_info_logged = false;
    bool decoded_any = false;
    size_t cached_len = 0;
    ret = ESP_AUDIO_ERR_OK;

    while (true) {
        size_t bytes_read = fread(in_buf + cached_len, 1, MP3_IN_BUF_SIZE - cached_len, file);
        if (bytes_read == 0 && cached_len == 0) {
            break;
        }

        esp_audio_dec_in_raw_t raw = {
            .buffer = in_buf,
            .len = (uint32_t)(cached_len + bytes_read),
        };
        cached_len = 0;

        while (raw.len > 0) {
            esp_audio_dec_out_frame_t out_frame = {
                .buffer = out_buf,
                .len = out_buf_size,
            };

            ret = esp_audio_dec_process(decoder, &raw, &out_frame);
            if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                uint8_t *new_out_buf = (uint8_t *)realloc(out_buf, out_frame.needed_size);
                if (new_out_buf == NULL) {
                    ESP_LOGE(TAG, "realloc output buffer failed, needed=%u",
                             (unsigned int)out_frame.needed_size);
                    ret = ESP_AUDIO_ERR_MEM_LACK;
                    goto cleanup;
                }

                out_buf = new_out_buf;
                out_buf_size = out_frame.needed_size;
                continue;
            }

            if (ret == ESP_AUDIO_ERR_DATA_LACK || ret == ESP_AUDIO_ERR_CONTINUE) {
                if (raw.consumed > 0 && raw.consumed <= raw.len) {
                    raw.buffer += raw.consumed;
                    raw.len -= raw.consumed;
                }

                if (raw.len > 0) {
                    memmove(in_buf, raw.buffer, raw.len);
                    cached_len = raw.len;
                }
                break;
            }

            if (ret != ESP_AUDIO_ERR_OK) {
                if (decoded_any && ret == ESP_AUDIO_ERR_NOT_SUPPORT) {
                    ESP_LOGW(TAG, "ignore trailing unsupported MP3 data");
                    ret = ESP_AUDIO_ERR_OK;
                    goto cleanup;
                }
                ESP_LOGE(TAG, "MP3 decode failed: %d", ret);
                goto cleanup;
            }

            if (!audio_info_logged) {
                esp_audio_dec_info_t info = {0};
                if (esp_audio_dec_get_info(decoder, &info) == ESP_AUDIO_ERR_OK) {
                    ESP_LOGI(TAG, "MP3 info: %lu Hz, %u channel, %u bit",
                             (unsigned long)info.sample_rate,
                             (unsigned int)info.channel,
                             (unsigned int)info.bits_per_sample);

                    if (info.sample_rate != 16000 || info.channel != 1 || info.bits_per_sample != 16) {
                        ESP_LOGW(TAG, "audio output is configured for 16 kHz, 16-bit, mono");
                    }
                }
                audio_info_logged = true;
            }

            if (out_frame.decoded_size > 0) {
                audio_write(out_frame.buffer, out_frame.decoded_size);
                decoded_any = true;
            }

            if (raw.consumed == 0) {
                break;
            }

            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }

        if (bytes_read == 0) {
            break;
        }
    }

    if (decoded_any && (ret == ESP_AUDIO_ERR_DATA_LACK || ret == ESP_AUDIO_ERR_CONTINUE)) {
        ret = ESP_AUDIO_ERR_OK;
    }

cleanup:
    free(in_buf);
    free(out_buf);
    esp_audio_dec_close(decoder);
    return ret;
}

void mp3_player_task(void *pvParameters)
{
    const char *file_name = (const char *)pvParameters;
    if (file_name == NULL) {
        file_name = DEFAULT_MP3_FILE;
    }

    char file_path[96] = {0};
    snprintf(file_path, sizeof(file_path), "%s/%s", MP3_BASE_PATH, file_name);

    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "failed to open MP3 file: %s", file_path);
        s_mp3_playing = false;
        if (s_playback_mutex != NULL) {
            xSemaphoreGive(s_playback_mutex);
        }
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "start MP3 playback: %s", file_path);
    esp_audio_err_t ret = mp3_decode_file(file);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "MP3 playback stopped with error: %d", ret);
    } else {
        ESP_LOGI(TAG, "MP3 playback finished");
    }

    fclose(file);
    s_mp3_playing = false;
    if (s_playback_mutex != NULL) {
        xSemaphoreGive(s_playback_mutex);
    }
    vTaskDelete(NULL);
}

esp_err_t audio_mp3_play_file_async(const char *file_name)
{
    if (file_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (com_status == LISTENING) {
        ESP_LOGW(TAG, "MultiNet listening, skip MP3: %s", file_name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = mount_storage_partition();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_playback_mutex == NULL) {
        s_playback_mutex = xSemaphoreCreateBinary();
        if (s_playback_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreGive(s_playback_mutex);
    }

    if (xSemaphoreTake(s_playback_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "MP3 playback busy, skip: %s", file_name);
        return ESP_ERR_INVALID_STATE;
    }

    s_mp3_playing = true;

    BaseType_t task_ret = xTaskCreate(mp3_player_task, "mp3_player", 4 * 1024, (void *)file_name, 4, NULL);
    if (task_ret != pdPASS) {
        s_mp3_playing = false;
        xSemaphoreGive(s_playback_mutex);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void audio_mp3_decode_task(void)
{
    if (mount_storage_partition() != ESP_OK) {
        return;
    }

    mp3_player_task((void *)DEFAULT_MP3_FILE);
}
