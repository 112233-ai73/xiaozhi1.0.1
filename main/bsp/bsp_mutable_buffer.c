#include "bsp_mutable_buffer.h"
#include "esp_heap_caps.h"
#include <string.h>
struct mutable_buffer
{
    char *data;
    int data_len;
};

// 创建一个可变的缓冲空间
mutable_buffer_t *bsp_mutable_buffer_create(void)
{
    // 使用cap申请空间
    mutable_buffer_t *buffer = (mutable_buffer_t *)heap_caps_malloc(sizeof(mutable_buffer_t), MALLOC_CAP_SPIRAM);

    // 设置初始化全部是0
    memset(buffer, 0, sizeof(mutable_buffer_t));

    // 返回
    return buffer;
}

// 向缓存添加数据
void bsp_mutable_buffer_add_data(mutable_buffer_t *buffer, void *data, size_t len)
{
    // 如果是第一次存放数据 则新申请内存用于存放数据 如果不是第一次 则需要扩容
    if (buffer->data_len == 0)
    {
        char *data_buffer = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM);
        buffer->data = data_buffer;
    }
    else
    {
        char *data_buffer = (char *)heap_caps_realloc(buffer->data, buffer->data_len + len + 1, MALLOC_CAP_SPIRAM);
        buffer->data = data_buffer;
    }

    // 将数据拷贝到新申请的空间中
    memcpy(buffer->data + buffer->data_len, data, len);
    buffer->data_len += len;
}

void *bsp_mutable_buffer_get_data(mutable_buffer_t *buffer)
{
    return buffer->data;
}

// 释放数据
void bsp_mutable_buffer_free(mutable_buffer_t *buffer)
{
    // 判断数据长度大于0
    if (buffer->data_len > 0)
    {
        // 释放数据
        heap_caps_free(buffer->data);
        // 设置数据长度为0
        buffer->data_len = 0;
    }
    free(buffer); // 释放结构体
}
