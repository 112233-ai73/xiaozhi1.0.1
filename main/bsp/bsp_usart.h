#ifndef __BSP_USART_H__
#define __BSP_USART_H__

#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"

#define UART_NUM       UART_NUM_0
#define UART_TX_IO     GPIO_NUM_43
#define UART_RX_IO     GPIO_NUM_44
#define UART_BAUD_RATE (115200)
#define UART_BUF_SIZE  (1024)
#define FRAME_HEADER_A 0xAA
#define FRAME_HEADER_B 0xBB

/**
 * @brief 初始化 USART 驱动、事件队列及接收任务
 */
void usart_init(void);

/**
 * @brief 发送原始数据包
 * @param data 包含长度及校验位的完整数据包指针
 */
void usart_send_data(const uint8_t *data);

#endif /* __BSP_USART_H__ */

