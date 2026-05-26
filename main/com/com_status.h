#ifndef __COM_STATUS_H__
#define __COM_STATUS_H__

#include <stdbool.h>

typedef enum
{
    START = 0,
    IDLE,
    LISTENING,
    SPEAKING,
} com_status_t;

extern com_status_t com_status;
extern bool is_awake;
extern bool MP3_after_awake;
extern bool is_wsline;


void com_set_awake(bool awake);
void com_awake_timeout_check(void);
void com_status_change(com_status_t status);

#endif /* __COM_STATUS_H__ */
