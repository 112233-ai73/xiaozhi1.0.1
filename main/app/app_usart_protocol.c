#include "app_usart_protocol.h"

#define WIFI_FRAGMENT_BYTES 15
#define WIFI_CREDENTIAL_BYTES 32
#define WIFI_CMD_SSID 0x00
#define WIFI_CMD_PASSWORD 0x02
#define WIFI_FRAGMENT_FIRST 0x01
#define WIFI_FRAGMENT_SECOND 0x02
#define WIFI_FRAGMENT_COMPLETE_MASK 0x0F
#define WIFI_FRAME_HEADER 0xAA

typedef struct
{
    uint8_t ssid[WIFI_CREDENTIAL_BYTES];
    uint8_t password[WIFI_CREDENTIAL_BYTES];
    uint8_t packet_mask;
} wifi_fragment_context_t;

static wifi_fragment_context_t s_wifi_fragments = {0};

static bool is_wifi_fragment_command(uint8_t cmd_id)
{
    return cmd_id == WIFI_CMD_SSID || cmd_id == WIFI_CMD_PASSWORD;
}

static bool is_wifi_fragment_packet(const uint8_t *packet)
{
    return packet[0] == WIFI_FRAME_HEADER && is_wifi_fragment_command(packet[2]);
}

static void wifi_fragment_reset(void)
{
    memset(&s_wifi_fragments, 0, sizeof(s_wifi_fragments));
}

static uint8_t *wifi_fragment_target_buffer(uint8_t cmd_id)
{
    return (cmd_id == WIFI_CMD_SSID) ? s_wifi_fragments.ssid : s_wifi_fragments.password;
}

static uint8_t wifi_fragment_bit_offset(uint8_t cmd_id)
{
    return (cmd_id == WIFI_CMD_SSID) ? 0 : 2;
}

static bool wifi_fragment_copy(uint8_t *target_buf, uint8_t seq, const uint8_t *data, uint8_t data_len)
{
    int start_index = (seq == WIFI_FRAGMENT_FIRST) ? 0 : WIFI_FRAGMENT_BYTES;
    bool early_stop = false;

    for (int i = 0; i < data_len && i < WIFI_FRAGMENT_BYTES; i++)
    {
        target_buf[start_index + i] = data[i];
        if (data[i] == 0x00)
        {
            early_stop = true;
            break;
        }
    }
    return early_stop;
}

static void wifi_fragment_update_mask(uint8_t cmd_id, uint8_t seq, bool early_stop)
{
    uint8_t bit_offset = wifi_fragment_bit_offset(cmd_id);

    if (seq == WIFI_FRAGMENT_FIRST)
    {
        s_wifi_fragments.packet_mask |= (1 << bit_offset);
        if (early_stop)
        {
            s_wifi_fragments.packet_mask |= (1 << (bit_offset + 1));
        }
    }
    else if (seq == WIFI_FRAGMENT_SECOND)
    {
        s_wifi_fragments.packet_mask |= (1 << (bit_offset + 1));
    }
}

static void wifi_fragment_connect_if_complete(void)
{
    if (s_wifi_fragments.packet_mask != WIFI_FRAGMENT_COMPLETE_MASK)
    {
        return;
    }

    s_wifi_fragments.ssid[WIFI_CREDENTIAL_BYTES - 1] = '\0';
    s_wifi_fragments.password[WIFI_CREDENTIAL_BYTES - 1] = '\0';

    MY_LOGI("WiFi puzzle complete! Triggering connection...");
    bsp_wifi_start_connect((const char *)s_wifi_fragments.ssid,
                           (const char *)s_wifi_fragments.password);
    s_wifi_fragments.packet_mask = 0x00;
}

static void process_wifi_fragments(uint8_t cmd_id, uint8_t seq, const uint8_t *data, uint8_t data_len)
{
    if (cmd_id == WIFI_CMD_SSID && seq == WIFI_FRAGMENT_FIRST)
    {
        wifi_fragment_reset();
    }

    uint8_t *target_buf = wifi_fragment_target_buffer(cmd_id);
    bool early_stop = wifi_fragment_copy(target_buf, seq, data, data_len);

    wifi_fragment_update_mask(cmd_id, seq, early_stop);
    wifi_fragment_connect_if_complete();
}

static void process_normal_command(const uint8_t *packet, uint8_t header, uint8_t cmd_id, const uint8_t *data, uint8_t data_len)
{
    (void)data;

    // 使用数学规律简化 0x6A ~ 0x76 的 MP3 异步播放指令
    if (header == 0xAA)
    {
        com_status_change(SPEAKING);
        switch (cmd_id)
        {
        case 0X6B:
            audio_mp3_play_file_async("200.mp3");
            break;
        case 0X6D:
            audio_mp3_play_file_async("201.mp3");
            break;
        case 0X6F:
            audio_mp3_play_file_async("202.mp3");
            break;
        case 0X71:
            audio_mp3_play_file_async("203.mp3");
            break;
        case 0X73:
            audio_mp3_play_file_async("204.mp3");
            break;
        case 0X75:
            audio_mp3_play_file_async("205.mp3");
            break;
        case 0X77:
            audio_mp3_play_file_async("206.mp3");
            break;
        case 0X80:
            audio_mp3_play_file_async("207.mp3");
            break;
        case 0X81:
            audio_mp3_play_file_async("208.mp3");
            break;
        default:
            MY_LOGI("unhandled USART command=0x%02X payload_len=%u", cmd_id, data_len);
            break;
        }
    }
    else if (header == 0xBB)
    {
        switch (cmd_id)
        {
        case 0x3E:
            if (packet[3] == 0x01)
            {
                
            }
            if (packet[3] == 0x02)
            {
               
            }
            break;
        case 0X99:
            MY_LOGI("无关任务挂起");
            app_sr_suspend_tasks();
            MY_LOGI("OTA 开始");
            start_xiaozhi_ota();
            break;
        default:
            MY_LOGI("unhandled USART command=0x%02X payload_len=%u", cmd_id, data_len);
            break;
        }
    }
}

static void dispatch_wifi_fragment_packet(uint8_t cmd_id, const uint8_t *packet, uint8_t packet_len)
{
    if (packet_len < 5)
    {
        MY_LOGW("invalid WiFi fragment packet length: cmd=0x%02X len=%u", cmd_id, packet_len);
        return;
    }

    uint8_t seq = packet[3];
    const uint8_t *data_payload = &packet[4];
    uint8_t data_len = packet_len - 5; // 减去 帧头(1) + 长度(1) + 命令(1) + 序号(1) + 校验(1)

    process_wifi_fragments(cmd_id, seq, data_payload, data_len);
}

static void dispatch_normal_packet(uint8_t header, uint8_t cmd_id, const uint8_t *packet, uint8_t packet_len)
{
    const uint8_t *data_payload = NULL;
    uint8_t data_len = 0;

    if (packet_len > 4)
    {
        data_payload = &packet[3];
        data_len = packet_len - 4; // 减去 帧头(1) + 长度(1) + 命令(1) + 校验(1)
    }

    process_normal_command(packet, header, cmd_id, data_payload, data_len);
}

void app_usart_protocol_handle_packet(const uint8_t *packet, uint8_t packet_len)
{
    if (packet == NULL || packet_len < 3)
    {
        return;
    }

    uint8_t header = packet[0];
    uint8_t cmd_id = packet[2];

    if (is_wifi_fragment_packet(packet))
    {
        dispatch_wifi_fragment_packet(cmd_id, packet, packet_len);
    }
    else
    {
        dispatch_normal_packet(header, cmd_id, packet, packet_len);
    }
}
