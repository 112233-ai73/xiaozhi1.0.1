#include <stdio.h>
#include "common/common_debug.h"
#include "audio/audio_init.h"

void app_main(void)
{
    MY_LOGE("ready to init audio");
    audio_init();
    MY_LOGE("audio init success");

    //创建缓冲区不断接收音频数据并交给扬声器
    uint8_t audio_buffer[1024]={0};

    while(1){
        audio_es7210_read_mic(audio_buffer, 1024);
        audio_es8311_write_speaker(audio_buffer, 1024);
        vTaskDelay(10);
    }
}
