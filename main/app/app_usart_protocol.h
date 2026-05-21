#ifndef __APP_USART_PROTOCOL_H__
#define __APP_USART_PROTOCOL_H__

#include <stdint.h>
#include "com/com_debug.h"
#include "bsp_wifi.h"
#include "audio_mp3_decode.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "ota.h"

/**
 * @brief 处理分发已经过校验的串口完整数据包
 * @param packet     完整数据包指针（包含帧头、长度、命令、载荷及校验位）
 * @param packet_len 数据包总长度
 */
void app_usart_protocol_handle_packet(const uint8_t *packet, uint8_t packet_len);

#endif /* __APP_USART_PROTOCOL_H__ */
