#ifndef __BSP_MUTABLE_BUFFER_H__
#define __BSP_MUTABLE_BUFFER_H__

#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct mutable_buffer mutable_buffer_t;

// 创建一个可变的缓冲空间
mutable_buffer_t *bsp_mutable_buffer_create(void);

// 向缓存添加数据
void bsp_mutable_buffer_add_data(mutable_buffer_t *buffer, void *data, size_t len);

// 获取缓存数据
void *bsp_mutable_buffer_get_data(mutable_buffer_t *buffer);

// 释放数据
void bsp_mutable_buffer_free(mutable_buffer_t *buffer);

#endif /* __BSP_MUTABLE_BUFFER_H__ */