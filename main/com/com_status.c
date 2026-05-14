#include "com_status.h"

com_status_t com_status = START;

char *com_status_str[] = {
    "START",
    "IDLE",
    "WORKING",
    "LISTERENING",
    "SPEAKING",
};

/**
 * @brief 改变状态
 *
 */
void com_status_change(com_status_t status)
{

    // 打印当前是什么状态,要变成什么状态
    MY_LOGE("com_status_change: %s -> %s", com_status_str[com_status], com_status_str[status]);

    com_status = status;
}