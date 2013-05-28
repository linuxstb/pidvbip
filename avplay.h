#ifndef _AVPLAY_H
#define _AVPLAY_H

#include "common.h"
#include "codec.h"
#include "msgqueue.h"

struct avplay_t
{
  struct msgqueue_t msgqueue;

  struct codecs_t* codecs;
  char* url;

  char* next_url;
  int64_t duration;
  int64_t PTS;

  pthread_t thread;
  pthread_mutex_t mutex;
  OMX_VIDEO_CODINGTYPE vcodectype;
  int width;
  int height;
  OMX_AUDIO_CODINGTYPE acodectype;
};

void init_avplay(struct avplay_t* avplay, struct codecs_t* codecs);

#endif
