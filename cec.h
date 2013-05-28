#ifndef _CEC_H
#define _CEC_H

#include "msgqueue.h"

int cec_init(struct msgqueue_t* msgqueue);
int cec_done(int poweroff);

#endif
