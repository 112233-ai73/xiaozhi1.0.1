#include "usart_init.h"
#include "com/com_debug.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

//static const char *TAG = "USART";

#define USART_LOG_HEX_MAX_BYTES 32
#define USART_MAX_PACKET_BYTES 255
#define USART_EVENT_QUEUE_SIZE 20
#define USART_EVENT_TASK_STACK_SIZE (4 * 1024)
#define USART_EVENT_TASK_PRIORITY 5
#define WIFI_FRAGMENT_BYTES 15
#define WIFI_CREDENTIAL_BYTES 32
#define WIFI_CMD_SSID 0x00
#define WIFI_CMD_PASSWORD 0x02
#define WIFI_FRAGMENT_FIRST 0x01
#define WIFI_FRAGMENT_SECOND 0x02
#define WIFI_FRAGMENT_COMPLETE_MASK 0x0F

typedef void (*usart_rx_callback_t)(void);

static QueueHandle_t s_uart_event_queue = NULL;
static usart_rx_callback_t s_rx_callback = NULL;

typedef struct
{
    uint8_t ssid[WIFI_CREDENTIAL_BYTES];
    uint8_t password[WIFI_CREDENTIAL_BYTES];
    uint8_t packet_mask;
} wifi_fragment_context_t;

static wifi_fragment_context_t s_wifi_fragments = {0};

static uint8_t calculate_xor_checksum(const uint8_t *data, size_t len)
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

static void append_hex_bytes(char *hex_buf, size_t hex_buf_size,
                             const uint8_t *data, uint8_t data_len)
{
    int offset = 0;

    for (uint8_t i = 0; i < data_len && offset < (int)hex_buf_size; i++)
    {
        offset += snprintf(hex_buf + offset, hex_buf_size - offset,
                           "%02X%s", data[i], (i + 1 < data_len) ? " " : "");
    }
}

static void usart_log_received_packet(const uint8_t *packet, uint8_t packet_len)
{
    char hex_buf[USART_MAX_PACKET_BYTES * 3 + 4] = {0};

    append_hex_bytes(hex_buf, sizeof(hex_buf), packet, packet_len);

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
    if (len < 3)
    {
        MY_LOGW("USART send ignored: invalid len=%u", len);
        return;
    }

    const uint8_t log_len = (len > USART_LOG_HEX_MAX_BYTES) ? USART_LOG_HEX_MAX_BYTES : len;
    char hex_buf[USART_LOG_HEX_MAX_BYTES * 3 + 4] = {0};

    append_hex_bytes(hex_buf, sizeof(hex_buf), data, log_len);
    if (log_len < len)
    {
        strlcat(hex_buf, "...", sizeof(hex_buf));
    }

    int written = uart_write_bytes(UART_NUM, data, len);
    MY_LOGI("USART send len=%u written=%d data=[%s]", len, written, hex_buf);
}

static bool is_wifi_fragment_command(uint8_t cmd_id)
{
    return cmd_id == WIFI_CMD_SSID || cmd_id == WIFI_CMD_PASSWORD;
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

static bool wifi_fragment_copy(uint8_t *target_buf, uint8_t seq,
                               const uint8_t *data, uint8_t data_len)
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

static bool usart_read_packet(uint8_t *packet, uint8_t *packet_len)
{
    uint8_t temp_byte;

    if (uart_read_bytes(UART_NUM, &temp_byte, 1, 0) <= 0)
    {
        return false;
    }

    if (temp_byte != FRAME_HEADER_CMD)
    {
        return false;
    }

    packet[0] = temp_byte;
    if (uart_read_bytes(UART_NUM, &packet[1], 1, pdMS_TO_TICKS(100)) <= 0)
    {
        return false;
    }

    *packet_len = packet[1];
    if (*packet_len < 3)
    {
        MY_LOGW("invalid packet length: %u", *packet_len);
        return false;
    }

    int remaining_len = *packet_len - 2;
    int read_res = uart_read_bytes(UART_NUM, &packet[2],
                                   remaining_len, pdMS_TO_TICKS(100));

    return read_res == remaining_len;
}

static bool usart_packet_checksum_passed(const uint8_t *packet, uint8_t packet_len)
{
    uint8_t calculated_xor = calculate_xor_checksum(packet, packet_len - 1);
    uint8_t received_xor = packet[packet_len - 1];

    if (calculated_xor != received_xor)
    {
        MY_LOGW("checksum failed: calculated=0x%02X, received=0x%02X",
                calculated_xor, received_xor);
        return false;
    }

    return true;
}

static void usart_dispatch_packet(uint8_t *packet, uint8_t packet_len)
{
    uint8_t cmd_id = packet[2];

    MY_LOGI("checksum passed, command=0x%02X", cmd_id);
    usart_log_received_packet(packet, packet_len);

    if (is_wifi_fragment_command(cmd_id) && packet_len >= 5)
    {
        uint8_t seq = packet[3];
        uint8_t *data_payload = &packet[4];
        uint8_t data_len = packet_len - 5;

        process_wifi_fragments(cmd_id, seq, data_payload, data_len);
    }
}

static void usart_receive_callback(void)
{
    uint8_t full_packet[UART_BUF_SIZE];
    size_t buffered_len = 0;

    while (uart_get_buffered_data_len(UART_NUM, &buffered_len) == ESP_OK && buffered_len > 0)
    {
        uint8_t packet_len = 0;

        if (!usart_read_packet(full_packet, &packet_len))
        {
            continue;
        }

        if (usart_packet_checksum_passed(full_packet, packet_len))
        {
            usart_dispatch_packet(full_packet, packet_len);
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
