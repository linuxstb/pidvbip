#ifndef _MSGQUEUE_T
#define _MSGQUEUE_T

#include <pthread.h>

#define MSGQUEUE_SIZE 64

struct msgqueue_t
{
  pthread_mutex_t mutex;
  pthread_cond_t count_cv;
  int count;
  int messages[MSGQUEUE_SIZE];
};

void msgqueue_init(struct msgqueue_t* q);
int msgqueue_get(struct msgqueue_t* q, int timeout_ms);
void msgqueue_add(struct msgqueue_t* q, int item);

#endif
