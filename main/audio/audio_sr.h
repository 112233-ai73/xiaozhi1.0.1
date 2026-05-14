#ifndef __AUDIO_SR_H__
#define __AUDIO_SR_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "audio_init.h"
#include "com/com_status.h"

#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"
#include "esp_wn_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "command_word.h"

#include <assert.h>

#include "model_path.h"
#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"

typedef struct {
    wakenet_state_t     wakenet_mode;
    esp_mn_state_t      state;
    int                 command_id;
    float               confidence;
} sr_result_t;

esp_err_t app_sr_start(void);

#endif /* __AUDIO_SR_H__ */
