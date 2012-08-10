
#include <stdio.h>
#include <pthread.h>
#include "codec.h"

void codec_queue_init(struct codec_t* codec)
{
  codec->queue_head = NULL;
  codec->queue_tail = NULL;
  codec->queue_count = 0;

  pthread_mutex_init(&codec->queue_mutex,NULL);
  pthread_cond_init(&codec->queue_count_cv,NULL);
}

void codec_queue_add_item(struct codec_t* codec, struct packet_t* packet)
{
  struct codec_queue_t* new = malloc(sizeof(struct codec_queue_t));

  if (new == NULL) {
    fprintf(stderr,"FATAL ERROR: out of memory adding to queue\n");
    exit(1);
  }

  pthread_mutex_lock(&codec->queue_mutex);

  if (codec->queue_head == NULL) {
    new->next = NULL;
    new->prev = NULL;
    new->data = packet;
    codec->queue_head = new;
    codec->queue_tail = new;

    pthread_cond_signal(&codec->queue_count_cv);
  } else {
    new->data = packet;
    new->next = codec->queue_head;
    new->prev = NULL;
    new->next->prev = new;
    codec->queue_head = new;
  }

  codec->queue_count++;

  pthread_mutex_unlock(&codec->queue_mutex);
}

void codec_queue_free_item(struct codec_t* codec,struct codec_queue_t* item)
{
  if (item == NULL)
    return;

  free(item->data->buf);
  free(item->data);
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

