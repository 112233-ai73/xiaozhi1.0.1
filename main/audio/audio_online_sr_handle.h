#ifndef __AUDIO_ONLINE_SR_HANDLE_H__
#define __AUDIO_ONLINE_SR_HANDLE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct audio_online_sr_handle audio_online_sr_handle_t;

audio_online_sr_handle_t *audio_online_sr_handle_create(void);
esp_err_t audio_online_sr_handle_start(audio_online_sr_handle_t *handle);
esp_err_t audio_online_init(void);
esp_err_t audio_online_start_async(void);
bool audio_online_is_ready(void);

esp_err_t audio_online_sr_handle_read_data(audio_online_sr_handle_t *handle,
                                           uint8_t *data,
                                           size_t data_size,
                                           size_t *read_size);

esp_err_t audio_online_sr_handle_write_data(audio_online_sr_handle_t *handle,
                                            const uint8_t *data,
                                            size_t size);

#endif /* __AUDIO_ONLINE_SR_HANDLE_H__ */
