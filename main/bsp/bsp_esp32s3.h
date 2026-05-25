#ifndef __BSP_ESP32S3_H__
#define __BSP_ESP32S3_H__

#include "esp_heap_caps.h"

#define UUID_KEY "UUID"
// 获取板子的mac地址函数
char *bsp_get_mac(void);

// 获取板子的uuid函数
char *bsp_get_uuid(void);

#endif /* __BSP_ESP32S3_H__ */
