#ifndef __USART_INIT_H__
#define __USART_INIT_H__

#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#define UART_NUM       UART_NUM_0     // 串口号：可选 UART_NUM_0, 1, 2
#define UART_TX_IO     GPIO_NUM_43          // TX 引脚号
#define UART_RX_IO     GPIO_NUM_44          //  RX 引脚号
#define UART_BAUD_RATE (115200)        // 波特率
#define UART_BUF_SIZE  (1024)          // 接收缓存区大小
#define FRAME_HEADER_CMD 0xAA

/**
 * @brief 初始化 USART 串口
 * 
 */
void usart_init(void);

/**
 * @brief 发送数据
 * 
 * @param data 待发送的数据缓冲区
 * @param len 数据长度
 */
void usart_send_data(const uint8_t *data);

/**
 * @brief 接收数据任务
 * 
 * @param pvParameters 任务参数
 */
void usart_receive_task(void *pvParameters);

#endif /* __USART_INIT_H__ */
