#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "codec.h"
#include "acodec_mpeg.h"

static void* acodec_mpeg_thread(struct codec_t* codec)
{
  struct codec_queue_t* current = NULL;
  FILE* fh;

  fh = popen("/usr/bin/mpg123 -","w");

  if (fh == NULL) {
    fprintf(stderr,"FATAL ERROR: Could not open mpg123\n");
    exit(1);
  }

  while (1) {
    current = codec_queue_get_next_item(codec);
    fwrite(current->data->packet,current->data->packetlength,1,fh);
    codec_queue_free_item(codec,current);
  }

  /* We never get here, but keep gcc happy */
  return NULL;
}

void acodec_mpeg_init(struct codec_t* codec)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_mpeg_thread,(void*)codec);
}


int64_t acodec_mpeg_current_get_pts(struct codec_t* codec)
{
}
