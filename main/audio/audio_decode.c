#include "audio_decode.h"

struct audio_decode
{
    // 解码器的输入缓冲区
    RingbufHandle_t input_buffer;
    // 解码器的输出缓冲区
    RingbufHandle_t output_buffer;
    // 解码器的句柄
    esp_audio_dec_handle_t decoder;
};

/**
 * @brief 解码器的任务函数
 */
static void audio_decode_task(void *pvParameters)
{
    // 1. 取出参数
    audio_decode_t *audio_decode = (audio_decode_t *)pvParameters;

    // 2. 注册解码器
    esp_opus_dec_register();

    // 3. 得到解码器的配置对象
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate = 16000,
        .channel = 1,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
    };
    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(opus_cfg),
    };
    // 4. 创建解码器
    esp_audio_dec_open(&dec_cfg, &audio_decode->decoder);

    // 5. 创建两个数据帧
    // 5.1 原始数据帧(opus数据帧),先定义好,可以暂时不写,等开始循环解码的时候,每次去拿值就行了
    esp_audio_dec_in_raw_t raw = {

    };

    // 5.2 先分配一个内存区域给到out_frame的buffer中,用来保存解码后的数据
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(1920, MALLOC_CAP_SPIRAM);
    esp_audio_dec_out_frame_t out_frame = {
        .buffer = buffer,
        .len = 1920,
    };

    // 6. 循环解码
    size_t len = 0;
    uint8_t *buf = NULL;
    while (1)
    {
        // 6.1 让raw去输入缓冲区中取数据
        buf = xRingbufferReceive(audio_decode->input_buffer, &len, portMAX_DELAY);
        raw.buffer = buf;
        raw.len = len;

        // 6.2 解码
        esp_audio_dec_process(audio_decode->decoder, &raw, &out_frame);

        // 6.3 把解码后的数据 交给输出缓冲区
        xRingbufferSend(audio_decode->output_buffer, out_frame.buffer, out_frame.decoded_size, portMAX_DELAY);

        // 6.4 释放输入缓冲区数据
        vRingbufferReturnItem(audio_decode->input_buffer, buf);
    }
}

/**
 * @brief 创建编码器结构体实例
 */
audio_decode_t *audio_decode_create()
{
    audio_decode_t *audio_decode = (audio_decode_t *)heap_caps_malloc(sizeof(audio_decode_t), MALLOC_CAP_SPIRAM);
    return audio_decode;
}

/**
 * @brief 启动解码器
 */
void audio_decode_start(audio_decode_t *audio_decode)
{
    // 创建任务 专门用来解码
    xTaskCreateWithCaps(audio_decode_task, "audio_decode_task", 32 * 1024, audio_decode, 10, NULL, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 注册输入缓冲区函数
 */
void audio_decode_register_input_ringbuf(audio_decode_t *audio_decode, RingbufHandle_t ringbuf_in)
{
    audio_decode->input_buffer = ringbuf_in;
}

/**
 * @brief 注册输出缓冲区函数
 */
void audio_decode_register_output_ringbuf(audio_decode_t *audio_decode, RingbufHandle_t ringbuf_out)
{
    audio_decode->output_buffer = ringbuf_out;
}
