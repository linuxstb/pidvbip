#ifndef _CEC_H
#define _CEC_H

#include "msgqueue.h"

int cec_init(int init_video, struct msgqueue_t* msgqueue);
int cec_get_keypress(void);
int cec_done(int poweroff);

#endif
