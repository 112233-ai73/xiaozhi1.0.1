#include "usart_init.h"

#include <stdio.h>

static const char *TAG = "USART";

#define USART_LOG_HEX_MAX_BYTES 32

/**
 * @brief 计算异或校验和
 * @param data 数据缓冲区
 * @param len 需要参与计算的数据长度（不含校验位本身）
 * @return 计算得到的异或结果
 */
static uint8_t calculate_xor_checksum(uint8_t *data, size_t len) {
    uint8_t xor_result = 0;
    for (size_t i = 0; i < len; i++) {
        xor_result ^= data[i];
    }
    return xor_result;
}

/**
 * @brief 初始化 USART 串口
 * 
 */
void usart_init(void)
{
    // 1. 配置 UART 参数
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,   // ESP32-S3 默认时钟源
    };

    // 2. 安装驱动程序
    // 参数：串口号, 接收缓存, 发送缓存（设为0则不开启硬件发送缓存）, 队列, 中断标志
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));

    // 3. 设置通讯引脚
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_IO, UART_RX_IO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

/**
 * @brief 发送数据
 * 
 * @param data 待发送的指令数组
 * 
 */
void usart_send_data(const uint8_t *data)
{
    if (data == NULL) {
        ESP_LOGW(TAG, "USART send ignored: data is NULL");
        return;
    }

    uint8_t len = data[1];
    if (len < 3 || len > UART_BUF_SIZE) {
        ESP_LOGW(TAG, "USART send ignored: invalid len=%u", len);
        return;
    }

    const uint8_t log_len = (len > USART_LOG_HEX_MAX_BYTES) ? USART_LOG_HEX_MAX_BYTES : len;
    char hex_buf[USART_LOG_HEX_MAX_BYTES * 3 + 4] = {0};
    int offset = 0;

    for (uint8_t i = 0; i < log_len && offset < (int)sizeof(hex_buf); i++) {
        offset += snprintf(hex_buf + offset, sizeof(hex_buf) - offset,
                           "%02X%s", data[i], (i + 1 < log_len) ? " " : "");
    }

    if (log_len < len && offset < (int)sizeof(hex_buf)) {
        snprintf(hex_buf + offset, sizeof(hex_buf) - offset, "...");
    }

    int written = uart_write_bytes(UART_NUM, data, len);
    ESP_LOGI(TAG, "USART send len=%u written=%d data=[%s]", len, written, hex_buf);
}

/**
 * @brief 接收数据任务
 * 
 * @param pvParameters 任务参数
 */
void usart_receive_task(void *pvParameters) {
    uint8_t temp_byte;
    uint8_t full_packet[UART_BUF_SIZE];
    static const char *TAG = "UART_PROTOCOL";

    while (1) {
        // 1. 寻找帧头 0xBB
        if (uart_read_bytes(UART_NUM, &temp_byte, 1, portMAX_DELAY) > 0) {
            if (temp_byte != FRAME_HEADER_BB) continue;

            full_packet[0] = temp_byte; // 存入帧头

            // 2. 读取长度位 (长度位在第2个字节)
            if (uart_read_bytes(UART_NUM, &full_packet[1], 1, pdMS_TO_TICKS(100)) <= 0) continue;
            
            uint8_t packet_len = full_packet[1];

            // 安全检查：防止长度位异常导致缓冲区溢出
            if (packet_len < 3) {
                ESP_LOGW(TAG, "非法的包长度: %d", packet_len);
                continue;
            }

            // 3. 读取剩余数据（包括命令、数据内容、以及最后的校验位）
            // 剩余字节数 = 总长度 - 已读的(帧头 + 长度位)
            int remaining_len = packet_len - 2; 
            int read_res = uart_read_bytes(UART_NUM, &full_packet[2], remaining_len, pdMS_TO_TICKS(100));

            if (read_res == remaining_len) {
                // 4. 进行异或校验
                // 计算范围：从 index 0 到 index (packet_len - 2)
                uint8_t calculated_xor = calculate_xor_checksum(full_packet, packet_len - 1);
                uint8_t received_xor = full_packet[packet_len - 1];

                if (calculated_xor == received_xor) {
                    ESP_LOGI(TAG, "校验通过！收到有效指令: 0x%02X", full_packet[2]);
                    // 在这里处理业务逻辑，例如解析 full_packet[2] 之后的 data
                } else {
                    ESP_LOGW(TAG, "校验失败！计算值: 0x%02X, 收到值: 0x%02X", calculated_xor, received_xor);
                }
            }
        }
    }
}
