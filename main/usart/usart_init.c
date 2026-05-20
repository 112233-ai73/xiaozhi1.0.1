#include "usart_init.h"
#include "com/com_debug.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>

//static const char *TAG = "USART";

#define USART_LOG_HEX_MAX_BYTES 32
#define USART_MAX_PACKET_BYTES 255
#define USART_EVENT_QUEUE_SIZE 20
#define USART_EVENT_TASK_STACK_SIZE (4 * 1024)
#define USART_EVENT_TASK_PRIORITY 5

typedef void (*usart_rx_callback_t)(void);

static QueueHandle_t s_uart_event_queue = NULL;
static usart_rx_callback_t s_rx_callback = NULL;

static uint8_t calculate_xor_checksum(uint8_t *data, size_t len)
{
    uint8_t xor_result = 0;
    for (size_t i = 0; i < len; i++)
    {
        xor_result ^= data[i];
    }
    return xor_result;
}

static void usart_register_rx_callback(usart_rx_callback_t callback)
{
    s_rx_callback = callback;
}

static void usart_log_received_packet(const uint8_t *packet, uint8_t packet_len)
{
    char hex_buf[USART_MAX_PACKET_BYTES * 3 + 4] = {0};
    int offset = 0;

    for (uint8_t i = 0; i < packet_len && offset < (int)sizeof(hex_buf); i++)
    {
        offset += snprintf(hex_buf + offset, sizeof(hex_buf) - offset,
                           "%02X%s", packet[i], (i + 1 < packet_len) ? " " : "");
    }

    ESP_LOGI(TAG, "USART recv command packet len=%u data=[%s]", packet_len, hex_buf);
}

void usart_send_data(const uint8_t *data)
{
    if (data == NULL)
    {
        MY_LOGW("USART send ignored: data is NULL");
        return;
    }

    uint8_t len = data[1];
    if (len < 3 || len > UART_BUF_SIZE)
    {
        MY_LOGW("USART send ignored: invalid len=%u", len);
        return;
    }

    const uint8_t log_len = (len > USART_LOG_HEX_MAX_BYTES) ? USART_LOG_HEX_MAX_BYTES : len;
    char hex_buf[USART_LOG_HEX_MAX_BYTES * 3 + 4] = {0};
    int offset = 0;

    for (uint8_t i = 0; i < log_len && offset < (int)sizeof(hex_buf); i++)
    {
        offset += snprintf(hex_buf + offset, sizeof(hex_buf) - offset,
                           "%02X%s", data[i], (i + 1 < log_len) ? " " : "");
    }

    if (log_len < len && offset < (int)sizeof(hex_buf))
    {
        snprintf(hex_buf + offset, sizeof(hex_buf) - offset, "...");
    }

    int written = uart_write_bytes(UART_NUM, data, len);
    MY_LOGI("USART send len=%u written=%d data=[%s]", len, written, hex_buf);
}

static void process_wifi_fragments(uint8_t cmd_id, uint8_t seq, const uint8_t *data, uint8_t data_len)
{
    // 使用 32 字节的数组，保证最后一位绝对能留给 '\0'
    static uint8_t wifi_ssid[32] = {0};
    static uint8_t wifi_pwd[32] = {0};
    static uint8_t wifi_packet_mask = 0x00;

    // 防卡死机制：收到第一包账号时，强制重置状态
    if (cmd_id == 0x00 && seq == 0x01)
    {
        wifi_packet_mask = 0x00;
        memset(wifi_ssid, 0, sizeof(wifi_ssid));
        memset(wifi_pwd, 0, sizeof(wifi_pwd));
    }

    uint8_t *target_buf = (cmd_id == 0x00) ? wifi_ssid : wifi_pwd;
    uint8_t bit_offset = (cmd_id == 0x00) ? 0 : 2;
    int start_index = (seq == 0x01) ? 0 : 15;
    bool early_stop = false;

    // 安全拷贝（遇 0x00 截断）
    for (int i = 0; i < data_len && i < 15; i++)
    {
        target_buf[start_index + i] = data[i];
        if (data[i] == 0x00)
        {
            early_stop = true;
            break;
        }
    }

    // 更新进度灯
    if (seq == 0x01)
    {
        wifi_packet_mask |= (1 << bit_offset);
        if (early_stop)
        {
            wifi_packet_mask |= (1 << (bit_offset + 1)); // 提前点亮第二包的灯
        }
    }
    else if (seq == 0x02)
    {
        wifi_packet_mask |= (1 << (bit_offset + 1));
    }

    // 检查拼图是否完成
    if (wifi_packet_mask == 0x0F)
    {
        // 安全锁：在数组最后强行补 '\0'
        wifi_ssid[31] = '\0';
        wifi_pwd[31] = '\0';

        MY_LOGI("WiFi puzzle complete! Triggering connection...");

        // 调用 WiFi 模块的接口，传入解析好的干净字符串
        bsp_wifi_start_connect((const char *)wifi_ssid, (const char *)wifi_pwd);

        // 清理现场
        wifi_packet_mask = 0x00;
    }
}

static void usart_receive_callback(void)
{
    uint8_t temp_byte;
    uint8_t full_packet[UART_BUF_SIZE];
    size_t buffered_len = 0;

    while (uart_get_buffered_data_len(UART_NUM, &buffered_len) == ESP_OK && buffered_len > 0)
    {
        if (uart_read_bytes(UART_NUM, &temp_byte, 1, 0) <= 0)
        {
            break;
        }

        if (temp_byte != FRAME_HEADER_CMD)
        {
            continue;
        }

        full_packet[0] = temp_byte;

        if (uart_read_bytes(UART_NUM, &full_packet[1], 1, pdMS_TO_TICKS(100)) <= 0)
        {
            continue;
        }

        uint8_t packet_len = full_packet[1];

        if (packet_len < 3)
        {
            MY_LOGW("invalid packet length: %u", packet_len);
            continue;
        }

        int remaining_len = packet_len - 2;
        int read_res = uart_read_bytes(UART_NUM, &full_packet[2],
                                       remaining_len, pdMS_TO_TICKS(100));

        if (read_res == remaining_len)
        {
            uint8_t calculated_xor = calculate_xor_checksum(full_packet, packet_len - 1);
            uint8_t received_xor = full_packet[packet_len - 1];

            if (calculated_xor == received_xor)
            {
                MY_LOGI("checksum passed, command=0x%02X", full_packet[2]);
                usart_log_received_packet(full_packet, packet_len);

                uint8_t cmd_id = full_packet[2];

                if (cmd_id == 0x00 || cmd_id == 0x02)
                {

                    if (packet_len >= 5)
                    {
                        uint8_t seq = full_packet[3];
                        uint8_t *data_payload = &full_packet[4];
                        uint8_t data_len = packet_len - 5;

                        process_wifi_fragments(cmd_id, seq, data_payload, data_len);
                    }
                }
                else if (cmd_id == 0x03)
                {
                }
            }
            else
            {
                MY_LOGW("checksum failed: calculated=0x%02X, received=0x%02X",
                        calculated_xor, received_xor);
            }
        }
    }
}

static void usart_event_task(void *pvParameters)
{
    (void)pvParameters;

    uart_event_t event;

    while (1)
    {
        if (xQueueReceive(s_uart_event_queue, &event, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        switch (event.type)
        {
        case UART_DATA:
            if (s_rx_callback != NULL)
            {
                s_rx_callback();
            }
            break;
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            MY_LOGW("USART rx buffer overflow, flushing input");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_event_queue);
            break;
        default:
            break;
        }
    }
}

void usart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0,
                                        USART_EVENT_QUEUE_SIZE,
                                        &s_uart_event_queue, 0));

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_IO, UART_RX_IO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    usart_register_rx_callback(usart_receive_callback);

    BaseType_t task_ret = xTaskCreate(usart_event_task, "usart_event",
                                      USART_EVENT_TASK_STACK_SIZE, NULL,
                                      USART_EVENT_TASK_PRIORITY, NULL);
    ESP_ERROR_CHECK(task_ret == pdPASS ? ESP_OK : ESP_FAIL);
}
