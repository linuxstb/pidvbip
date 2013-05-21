/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "common.h"

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#include "msgqueue.h"

void msgqueue_init(struct msgqueue_t* q)
{
  pthread_mutex_init(&q->mutex,NULL);
  pthread_cond_init(&q->count_cv,NULL);
  q->count = 0;
  memset(q->messages,0,sizeof(q->messages));
}

int msgqueue_get(struct msgqueue_t* q, int timeout_ms)
{
  int item;
  int rc;
  struct timespec ts;
  struct timeval tv;

  pthread_mutex_lock(&q->mutex);

  if (timeout_ms) {
    gettimeofday(&tv, NULL);

    /* Convert from timeval to timespec */
    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;

    /* Add timeout value and normalise */
    ts.tv_nsec += timeout_ms * 1000000;
    while (ts.tv_nsec > 1000000000) {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000;
    }

    while (q->count == 0) {
      rc = pthread_cond_timedwait(&q->count_cv, &q->mutex, &ts);
      if (rc == ETIMEDOUT) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
      }
    }
  } else {
    if (q->count == 0) {
      pthread_mutex_unlock(&q->mutex);
      return -1;
    }
  }

  item = q->messages[0];
  q->count--;
  if (q->count > 0) {
    memmove(q->messages, q->messages+1, q->count*sizeof(q->messages[0]));
  }

  pthread_mutex_unlock(&q->mutex);

  return item;
}
  
void msgqueue_add(struct msgqueue_t* q, int item)
{
  pthread_mutex_lock(&q->mutex);

  if (q->count < MSGQUEUE_SIZE) {
    q->messages[q->count++] = item;

    if (q->count==1) {
      pthread_cond_signal(&q->count_cv);
    }
  } else {
    fprintf(stderr,"[ERROR] Message queue full, dropping item %d\n",item);
  }

  pthread_mutex_unlock(&q->mutex);
}
