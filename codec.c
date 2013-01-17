
#include <stdio.h>
#include <pthread.h>
#include "codec.h"
#include "debug.h"

void codec_queue_init(struct codec_t* codec)
{
  codec->queue_head = NULL;
  codec->queue_tail = NULL;
  codec->queue_count = 0;
  codec->is_running = 1;
  codec->PTS = -1;

  pthread_mutex_init(&codec->queue_mutex,NULL);
  pthread_cond_init(&codec->queue_count_cv,NULL);
  pthread_cond_init(&codec->resume_cv,NULL);
  pthread_mutex_init(&codec->PTS_mutex,NULL);
  pthread_mutex_init(&codec->isrunning_mutex,NULL);
}

int codec_is_running(struct codec_t* codec)
{
  int res;
  pthread_mutex_lock(&codec->isrunning_mutex);

  res = codec->is_running;

  pthread_mutex_unlock(&codec->isrunning_mutex);

  return res;
}

void codec_flush_queue(struct codec_t* codec)
{
  /* Empty the queue */

  pthread_mutex_lock(&codec->queue_mutex);

  struct codec_queue_t* p = codec->queue_head;
  while (p) {
    struct codec_queue_t* tmp = p;
    p = p->next;
    codec_queue_free_item(codec,tmp);
  }

  codec->queue_count = 0;
  codec->queue_head = NULL;
  codec->queue_tail = NULL;

  pthread_mutex_unlock(&codec->queue_mutex);
}

void codec_stop(struct codec_t* codec)
{
  struct codec_queue_t* new = malloc(sizeof(struct codec_queue_t));

  if (new == NULL) {
    fprintf(stderr,"FATAL ERROR: out of memory adding to queue\n");
    exit(1);
  }

  pthread_mutex_lock(&codec->queue_mutex);

  pthread_mutex_lock(&codec->isrunning_mutex);
  codec->is_running = 0;
  pthread_mutex_unlock(&codec->isrunning_mutex);

  /* Empty the queue */

  struct codec_queue_t* p = codec->queue_head;
  while (p) {
    struct codec_queue_t* tmp = p;
    p = p->next;
    codec_queue_free_item(codec,tmp);
  }

  new->msgtype = MSG_STOP;
  new->data = NULL;
  new->next = NULL;
  new->prev = NULL;
  codec->queue_head = new;
  codec->queue_tail = new;

  if (codec->queue_count == 0) {
    pthread_cond_signal(&codec->queue_count_cv);
  }
  codec->queue_count=1;

  codec_set_pts(codec,-1);
  pthread_mutex_unlock(&codec->queue_mutex);
}

void codec_pause(struct codec_t* codec)
{
  struct codec_queue_t* new = malloc(sizeof(struct codec_queue_t));

  if (new == NULL) {
    fprintf(stderr,"FATAL ERROR: out of memory adding to queue\n");
    exit(1);
  }
  new->msgtype = MSG_PAUSE;
  new->data = NULL;

  pthread_mutex_lock(&codec->queue_mutex);

  /* End to end of queue */
  if (codec->queue_tail == NULL) {
    new->next = NULL;
    new->prev = NULL;
    codec->queue_head = new;
    codec->queue_tail = new;

    pthread_cond_signal(&codec->queue_count_cv);
  } else {
    new->next = NULL;
    new->prev = codec->queue_tail;
    new->prev->next = new;
    codec->queue_tail = new;
  }

  codec->queue_count++;

  pthread_mutex_unlock(&codec->queue_mutex);
}

void codec_resume(struct codec_t* codec)
{
  pthread_cond_signal(&codec->resume_cv);
}

void codec_queue_add_item(struct codec_t* codec, struct packet_t* packet)
{
  if (packet == NULL) {
    fprintf(stderr,"ERROR: Adding NULL packet to queue, skipping\n");
    return;
  }

  struct codec_queue_t* new = malloc(sizeof(struct codec_queue_t));

  if (new == NULL) {
    fprintf(stderr,"FATAL ERROR: out of memory adding to queue\n");
    exit(1);
  }

  new->msgtype = MSG_PACKET;
  new->data = packet;

  pthread_mutex_lock(&codec->queue_mutex);

  if (codec->is_running) {
    if (codec->queue_head == NULL) {
      new->next = NULL;
      new->prev = NULL;
      codec->queue_head = new;
      codec->queue_tail = new;

      pthread_cond_signal(&codec->queue_count_cv);
    } else {
      new->next = codec->queue_head;
      new->prev = NULL;
      new->next->prev = new;
      codec->queue_head = new;
    }

    codec->queue_count++;
  } else {
    DEBUGF("Dropping packet - codec is stopped.\n");
    free(packet);
  }

  pthread_mutex_unlock(&codec->queue_mutex);
}

void codec_queue_free_item(struct codec_t* codec,struct codec_queue_t* item)
{
  if (item == NULL)
    return;

  if (item->data) {
    free(item->data->buf);
    free(item->data);
  }
  free(item);
}

struct codec_queue_t* codec_queue_get_next_item(struct codec_t* codec)
{
  struct codec_queue_t* item;
  pthread_mutex_lock(&codec->queue_mutex);

  if (codec->queue_tail == NULL) {
    pthread_cond_wait(&codec->queue_count_cv,&codec->queue_mutex);
  }

  item = codec->queue_tail;

  codec->queue_tail = codec->queue_tail->prev;
  if (codec->queue_tail == NULL) {
    codec->queue_head = NULL;
  } else {
    codec->queue_tail->next = NULL;
  }

  codec->queue_count--;

  pthread_mutex_unlock(&codec->queue_mutex);

  return item;
}


void codec_set_pts(struct codec_t* codec, int64_t PTS)
{
  pthread_mutex_lock(&codec->PTS_mutex);
  codec->PTS = PTS;
  pthread_mutex_unlock(&codec->PTS_mutex);
}

int64_t codec_get_pts(struct codec_t* codec)
{
  int64_t PTS;

  pthread_mutex_lock(&codec->PTS_mutex);
  PTS = codec->PTS;
  pthread_mutex_unlock(&codec->PTS_mutex);

  return PTS;
}
