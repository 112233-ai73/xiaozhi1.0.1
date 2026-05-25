#include "audio_encode.h"

struct audio_encode
{
    // 编码器的输入缓冲区
    RingbufHandle_t input_buffer;
    // 编码器的输出缓冲区
    RingbufHandle_t output_buffer;
    // 编码器句柄
    esp_audio_enc_handle_t encoder;
};

/**
 * @brief 从环形缓冲区读取固定长度数据
 */
static void audio_encode_read_buffer_fix_len(RingbufHandle_t input_buffer, uint8_t *pcm_data, int pcm_size)
{
    size_t len = 0;
    uint8_t *data;
    while (pcm_size > 0)
    {
        data = xRingbufferReceiveUpTo(input_buffer, &len, portMAX_DELAY, pcm_size);

        // 把当前读取到的数据保存在pcm_data中
        memcpy(pcm_data, data, len);

        // 把要读取的最大值 进行累减处理 计算下次读取的最大值
        pcm_size -= len;
        pcm_data += len;

        // 释放缓冲区
        vRingbufferReturnItem(input_buffer, data);
    }
}

/**
 * @brief 编码器任务函数
 */
static void audio_encode_task(void *arg)
{
    // 拿到参数
    audio_encode_t *audio_encode = (audio_encode_t *)arg;

    // 1. 注册一个编码器
    esp_opus_enc_register();

    // 2. 拿到opus配置和编码器配置对象
    esp_opus_enc_config_t opus_cfg = {
        .sample_rate = ESP_AUDIO_SAMPLE_RATE_16K,
        .channel = ESP_AUDIO_MONO,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = 90000,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO,
        .complexity = 5,
        .enable_dtx = false,
        .enable_fec = false,
        .enable_vbr = false,

    };
    esp_audio_enc_config_t enc_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(opus_cfg)};

    // 3. 创建编码器,拿到编码器句柄
    esp_audio_enc_open(&enc_cfg, &audio_encode->encoder);

    // 4. 定义两个变量,pcm_size就是根据你的配置信息保存一个被处理的数据帧长度,raw_size就是处理后的数据长度
    int pcm_size = 0, raw_size = 0;
    esp_audio_enc_get_frame_size(audio_encode->encoder, &pcm_size, &raw_size);

    // 5. 创建两个缓冲区,一个用于保存原始数据,一个用于保存处理后的数据
    uint8_t *pcm_data = (uint8_t *)heap_caps_malloc(pcm_size, MALLOC_CAP_SPIRAM);
    uint8_t *raw_data = (uint8_t *)heap_caps_malloc(raw_size, MALLOC_CAP_SPIRAM);

    // 5. 定义两个数据帧,一个是据帧用于保存原始数据,一个数据帧用于保存处理后的数据
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = pcm_data,
        .len = pcm_size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = raw_data,
        .len = raw_size,
    };

    // 6. 持续编码
    while (1)
    {
        // 6.1 每次都要重新去SR的输出缓冲区(编码器的输入缓冲区)拿数据  写入pcm_data
        // 根据我们前边的配置,编码器每次要处理的一帧数据长度是pcm_size(1920)
        // 所以我们必须在编码之前 给pcm_size写入1920的数据 才能接下编码
        // pcm_data = xRingbufferReceive(audio_encode->input_buffer, );
        audio_encode_read_buffer_fix_len(audio_encode->input_buffer, pcm_data, pcm_size);

        // 6.2 启动编码器开始处理当前in_frame帧的数据
        esp_audio_enc_process(audio_encode->encoder, &in_frame, &out_frame);

        // 6.3 把编码后的结果拿走,写入到编码器的输出缓冲区
        xRingbufferSend(audio_encode->output_buffer, out_frame.buffer, out_frame.encoded_bytes, portMAX_DELAY);
    }
}

/**
 * @brief 创建一个编码器结构体实例
 */
audio_encode_t *audio_encode_create()
{
    audio_encode_t *audio_encode = (audio_encode_t *)heap_caps_malloc(sizeof(audio_encode_t), MALLOC_CAP_SPIRAM);

    return audio_encode;
}

/**
 * @brief 启动编码器
 */
void audio_encode_start(audio_encode_t *audio_encode)
{
    // 创建任务,持续进行编码
    xTaskCreateWithCaps(audio_encode_task, "audio_encode_task", 32 * 1024, audio_encode, 10, NULL, MALLOC_CAP_SPIRAM);
}

/**
 * @brief 给编码器注册输入缓冲区
 */
void audio_encode_register_input_ringbuf(audio_encode_t *audio_encode, RingbufHandle_t ringbuf_in)
{
    audio_encode->input_buffer = ringbuf_in;
}

/**
 * @brief 给编码器注册输出缓冲区
 */
void audio_encode_register_output_ringbuf(audio_encode_t *audio_encode, RingbufHandle_t ringbuf_out)
{
    audio_encode->output_buffer = ringbuf_out;
}
