#ifndef __COM_STATUS_H__
#define __COM_STATUS_H__

typedef enum
{
    START = 0,   // 默认起始状态
    IDLE,        // 空闲状态 表示准备工作已经完毕
    WORKING,     // 正在工作状态,已唤醒
    LISTERENING, // 正在监听状态
    SPEAKING,    // 正在说话状态
} com_status_t;

extern com_status_t com_status;

/**
 * @brief 改变状态
 *
 */
void com_status_change(com_status_t status);

#endif /* __COM_STATUS_H__ */
