#include "audio_mp3_decode.h"
#include "com/com_status.h"
#include "com/com_debug.h"

//static const char *TAG = "AUDIO_MP3_DECODE";

#define MP3_IN_BUF_SIZE  (2048)
#define PCM_OUT_BUF_SIZE (4096)
#define MP3_BASE_PATH    "/spiffs"
#define MP3_PARTITION    "mp3"
#define DEFAULT_MP3_FILE "/spiffs/test.mp3"
#define MP3_FILE_PATH_MAX_LEN 128

static SemaphoreHandle_t s_playback_mutex = NULL;
static volatile bool s_mp3_playing = false;
static volatile bool s_mp3_stop_requested = false;

typedef struct {
    char file_path[MP3_FILE_PATH_MAX_LEN];
    bool pause_multinet;
} mp3_play_request_t;

static bool is_spiffs_file_path(const char *file_path)
{
    return strncmp(file_path, MP3_BASE_PATH "/", strlen(MP3_BASE_PATH "/")) == 0;
}

bool audio_mp3_is_playing(void)
{
    return s_mp3_playing;
}

void stop_play_mp3(void)
{
    while (s_mp3_playing) {
        s_mp3_stop_requested = true;
    }  
}

esp_err_t mount_storage_partition(void)
{
    MY_LOGI("mounting SPIFFS partition...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = MP3_BASE_PATH,
        .partition_label = MP3_PARTITION,
        .max_files = 5,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_ERR_INVALID_STATE) {
        MY_LOGW("SPIFFS already mounted");
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            MY_LOGE("failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            MY_LOGE("failed to find SPIFFS partition: %s", MP3_PARTITION);
        } else {
            MY_LOGE("SPIFFS init failed: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(MP3_PARTITION, &total, &used);
    if (ret == ESP_OK) {
        MY_LOGI("SPIFFS size: total=%u KB, used=%u KB",
                 (unsigned int)(total / 1024),
                 (unsigned int)(used / 1024));
    }

    return ESP_OK;
}

// [修改] 增加了一个 enable_resample 参数
static esp_audio_err_t mp3_decode_file(FILE *file, bool enable_resample) 
{
    esp_audio_err_t ret = esp_audio_dec_register_default();
    if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_ALREADY_EXIST) {
        MY_LOGE("register audio decoders failed: %d", ret);
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
        MY_LOGE("open MP3 decoder failed: %d", ret);
        return ret;
    }

    uint8_t *in_buf = (uint8_t *)malloc(MP3_IN_BUF_SIZE);
    uint8_t *out_buf = (uint8_t *)malloc(PCM_OUT_BUF_SIZE);
    uint32_t out_buf_size = PCM_OUT_BUF_SIZE;

    // [新增] 软重采样相关的变量状态
    int16_t *resample_buf = NULL; 
    float resample_phase = 0.0f;  // 用于在多次 buffer 处理间平滑过渡相位
    bool need_resample = false;   
    int in_rate = 16000;
    int in_channels = 1;

    if (in_buf == NULL || out_buf == NULL) {
        MY_LOGE("malloc MP3 buffers failed");
        free(in_buf);
        free(out_buf);
        esp_audio_dec_close(decoder);
        return ESP_AUDIO_ERR_MEM_LACK;
    }

    bool audio_info_logged = false;
    bool decoded_any = false;
    size_t cached_len = 0;
    ret = ESP_AUDIO_ERR_OK;

    while (!s_mp3_stop_requested) {
        size_t bytes_read = fread(in_buf + cached_len, 1, MP3_IN_BUF_SIZE - cached_len, file);
        if (bytes_read == 0 && cached_len == 0) {
            break;
        }

        esp_audio_dec_in_raw_t raw = {
            .buffer = in_buf,
            .len = (uint32_t)(cached_len + bytes_read),
        };
        cached_len = 0;

        while (raw.len > 0 && !s_mp3_stop_requested) {
            esp_audio_dec_out_frame_t out_frame = {
                .buffer = out_buf,
                .len = out_buf_size,
            };

            ret = esp_audio_dec_process(decoder, &raw, &out_frame);
            if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                uint8_t *new_out_buf = (uint8_t *)realloc(out_buf, out_frame.needed_size);
                if (new_out_buf == NULL) {
                    MY_LOGE("realloc output buffer failed, needed=%u",
                             (unsigned int)out_frame.needed_size);
                    ret = ESP_AUDIO_ERR_MEM_LACK;
                    goto cleanup;
                }

                out_buf = new_out_buf;
                out_buf_size = out_frame.needed_size;

                // [新增] 同步扩容重采样缓冲区
                if (resample_buf != NULL) {
                    int16_t *new_resample = (int16_t *)realloc(resample_buf, out_buf_size);
                    if (new_resample != NULL) {
                        resample_buf = new_resample;
                    }
                }
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
                    MY_LOGW("ignore trailing unsupported MP3 data");
                    ret = ESP_AUDIO_ERR_OK;
                    goto cleanup;
                }
                MY_LOGE("MP3 decode failed: %d", ret);
                goto cleanup;
            }

            if (!audio_info_logged) {
                esp_audio_dec_info_t info = {0};
                if (esp_audio_dec_get_info(decoder, &info) == ESP_AUDIO_ERR_OK) {
                    MY_LOGI("MP3 info: %lu Hz, %u channel, %u bit",
                             (unsigned long)info.sample_rate,
                             (unsigned int)info.channel,
                             (unsigned int)info.bits_per_sample);

                    in_rate = info.sample_rate;
                    in_channels = info.channel;

                    // [新增] 判断是否需要介入重采样
                    if (enable_resample && (in_rate != 16000 || in_channels != 1)) {
                        MY_LOGI("Enable software resampling: %dHz %dCh -> 16000Hz 1Ch", in_rate, in_channels);
                        need_resample = true;
                        // 分配重采样空间（由于是降采样且变单声道，输出数据必定比原 buffer 小，使用 out_buf_size 足够安全）
                        resample_buf = (int16_t *)malloc(out_buf_size); 
                    } else if (in_rate != 16000 || in_channels != 1 || info.bits_per_sample != 16) {
                        MY_LOGW("audio output is configured for 16 kHz, 16-bit, mono (Resampling bypassed)");
                    }
                }
                audio_info_logged = true;
            }

            if (out_frame.decoded_size > 0 && !s_mp3_stop_requested) {
                // [新增] 核心重采样与降混音流
                if (need_resample && resample_buf != NULL) {
                    size_t in_samples = out_frame.decoded_size / sizeof(int16_t);
                    size_t in_frames = in_samples / in_channels;
                    size_t out_frames = 0;
                    float step = (float)in_rate / 16000.0f; 
                    
                    const int16_t *in_pcm = (const int16_t *)out_frame.buffer;
                    
                    while ((int)resample_phase < in_frames) {
                        int idx = (int)resample_phase;
                        int32_t mono_sample = 0;
                        
                        // 多声道求均值转单声道
                        for (int c = 0; c < in_channels; c++) {
                            mono_sample += in_pcm[idx * in_channels + c];
                        }
                        mono_sample /= in_channels;
                        
                        resample_buf[out_frames++] = (int16_t)mono_sample;
                        resample_phase += step;
                    }
                    
                    // 将相位偏移扣除当前帧数，保留余数留到下一次 decode 循环使用，防止音频跨帧时产生杂音毛刺
                    resample_phase -= in_frames; 
                    
                    audio_write((uint8_t *)resample_buf, out_frames * sizeof(int16_t));
                } else {
                    // 原生逻辑（非/sdcard 或已经是 16k 单声道）
                    audio_write(out_frame.buffer, out_frame.decoded_size);
                }
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

    if (s_mp3_stop_requested) {
        ret = ESP_AUDIO_ERR_OK;
    } else if (decoded_any && (ret == ESP_AUDIO_ERR_DATA_LACK || ret == ESP_AUDIO_ERR_CONTINUE)) {
        ret = ESP_AUDIO_ERR_OK;
    }

cleanup:
    free(in_buf);
    free(out_buf);
    if (resample_buf) {
        free(resample_buf); // [新增] 释放重采样内存
    }
    esp_audio_dec_close(decoder);
    return ret;
}

static void mp3_player_task(void *pvParameters)
{
    mp3_play_request_t *request = (mp3_play_request_t *)pvParameters;
    const char *file_path = request != NULL ? request->file_path : NULL;
    bool pause_multinet = request != NULL && request->pause_multinet;

    if (file_path == NULL || file_path[0] == '\0') {
        file_path = DEFAULT_MP3_FILE;
    }

    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        MY_LOGE("failed to open MP3 file: %s", file_path);
        if (pause_multinet) {
            app_sr_set_multinet_enabled(true);
        }
        free(request);
        s_mp3_playing = false;
        if (s_playback_mutex != NULL) {
            xSemaphoreGive(s_playback_mutex);
        }
        vTaskDelete(NULL);
        return;
    }

    // [新增] 判断是否为 sdcard 下的文件来决定是否挂载重采样
    bool enable_resample = false;
    if (strncmp(file_path, "/sdcard", 7) == 0) {
        enable_resample = true;
    }

    MY_LOGI("start MP3 playback: %s", file_path);
    // [修改] 将 enable_resample 传入解码函数
    esp_audio_err_t ret = mp3_decode_file(file, enable_resample); 
    
    if (s_mp3_stop_requested) {
        MY_LOGI("MP3 playback stopped");
        com_status_change(IDLE);
    } else if (ret != ESP_AUDIO_ERR_OK) {
        MY_LOGE("MP3 playback stopped with error: %d", ret);
    } else {
        MY_LOGI("MP3 playback finished");
        com_status_change(IDLE);
    }

    fclose(file);
    if (pause_multinet) {
        app_sr_set_multinet_enabled(true);
    }
    free(request);
    s_mp3_stop_requested = false;
    s_mp3_playing = false;
    if (s_playback_mutex != NULL) {
        xSemaphoreGive(s_playback_mutex);
    }
    vTaskDelete(NULL);
}

// ... 下方的 audio_mp3_play_file_async_internal 等其他函数保持不变 ...
static esp_err_t audio_mp3_play_file_async_internal(const char *file_name, bool pause_multinet)
{
    if (file_name == NULL || file_name[0] != '/') {
        MY_LOGE("MP3 file path must be absolute: %s", file_name == NULL ? "NULL" : file_name);
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(file_name) >= MP3_FILE_PATH_MAX_LEN) {
        MY_LOGE("MP3 file path too long: %s", file_name);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = ESP_OK;
    if (is_spiffs_file_path(file_name)) {
        ret = mount_storage_partition();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (s_playback_mutex == NULL) {
        s_playback_mutex = xSemaphoreCreateBinary();
        if (s_playback_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
        xSemaphoreGive(s_playback_mutex);
    }

    if (xSemaphoreTake(s_playback_mutex, 0) != pdTRUE) {
        MY_LOGW("MP3 playback busy, skip: %s", file_name);
        return ESP_ERR_INVALID_STATE;
    }

    s_mp3_playing = true;
    s_mp3_stop_requested = false;

    mp3_play_request_t *request = (mp3_play_request_t *)calloc(1, sizeof(mp3_play_request_t));
    if (request == NULL) {
        s_mp3_playing = false;
        xSemaphoreGive(s_playback_mutex);
        return ESP_ERR_NO_MEM;
    }
    snprintf(request->file_path, sizeof(request->file_path), "%s", file_name);
    request->pause_multinet = pause_multinet;

    if (pause_multinet) {
        app_sr_set_multinet_enabled(false);
    }

    BaseType_t task_ret = xTaskCreate(mp3_player_task, "mp3_player", 4 * 1024, request, 4, NULL);
    if (task_ret != pdPASS) {
        if (pause_multinet) {
            app_sr_set_multinet_enabled(true);
        }
        free(request);
        s_mp3_playing = false;
        xSemaphoreGive(s_playback_mutex);
        return ESP_FAIL;
    }
    
    // 假设这是你自定义的全局变量逻辑
    // if (MP3_after_awake) {
    //     com_set_awake(true);
    //     MP3_after_awake = false;
    // }

    return ESP_OK;
}

esp_err_t audio_mp3_play_file_async(const char *file_name)
{
    return audio_mp3_play_file_async_internal(file_name, false);
}

esp_err_t audio_mp3_play_file_async_without_multinet(const char *file_name)
{
    return audio_mp3_play_file_async_internal(file_name, true);
}

void audio_mp3_decode_task(void)
{
    if (mount_storage_partition() != ESP_OK) {
        return;
    }

    mp3_play_request_t *request = (mp3_play_request_t *)calloc(1, sizeof(mp3_play_request_t));
    if (request == NULL) {
        MY_LOGE("malloc MP3 request failed");
        return;
    }

    snprintf(request->file_path, sizeof(request->file_path), "%s", DEFAULT_MP3_FILE);
    mp3_player_task(request);
}