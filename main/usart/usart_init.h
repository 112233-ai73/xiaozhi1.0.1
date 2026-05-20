#ifndef __USART_INIT_H__
#define __USART_INIT_H__

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
#define FRAME_HEADER_CMD 0xAA

void usart_init(void);

void usart_send_data(const uint8_t *data);

#endif /* __USART_INIT_H__ */
