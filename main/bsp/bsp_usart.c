#include "bsp_usart.h"
#include "com/com_debug.h"
#include "app_usart_protocol.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define USART_LOG_HEX_MAX_BYTES 32
#define USART_MAX_PACKET_BYTES 255
#define USART_EVENT_QUEUE_SIZE 20
#define USART_EVENT_TASK_STACK_SIZE (4 * 1024)
#define USART_EVENT_TASK_PRIORITY 5

typedef void (*usart_rx_callback_t)(void);

static QueueHandle_t s_uart_event_queue = NULL;
static usart_rx_callback_t s_rx_callback = NULL;

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

static bool usart_read_packet(uint8_t *packet, uint8_t *packet_len)
{
    uint8_t temp_byte;

    if (uart_read_bytes(UART_NUM, &temp_byte, 1, 0) <= 0)
    {
        return false;
    }

    if (temp_byte != FRAME_HEADER_A && temp_byte != FRAME_HEADER_B)
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

static void usart_dispatch_packet(const uint8_t *packet, uint8_t packet_len)
{
    MY_LOGI("checksum passed, command=0x%02X", packet[2]);
    usart_log_received_packet(packet, packet_len);

    app_usart_protocol_handle_packet(packet, packet_len);
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
