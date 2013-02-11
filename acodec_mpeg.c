
/* MPEG audio codec using libmpg123.  Based on mpglib.c sample from libmpg123

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <mpg123.h>
#include "codec.h"
#include "acodec_mpeg.h"
#include "omx_utils.h"
#include "debug.h"

static void* acodec_mpeg_thread(struct codec_init_args_t* args)
{
  struct codec_t* codec = args->codec;
  struct omx_pipeline_t* pipe = args->pipe;
  struct codec_queue_t* current = NULL;
  size_t size;
  mpg123_handle *m;
  int ret;
  int is_paused = 0;

  OMX_BUFFERHEADERTYPE *buf;
  int first_packet = 1;

  free(args);
  mpg123_init();

  m = mpg123_new(NULL, &ret);

  if(m == NULL)
  {
    fprintf(stderr,"Unable to create mpg123 handle: %s\n", mpg123_plain_strerror(ret));
    exit(1);
  }
  mpg123_param(m, MPG123_VERBOSE, 2, 0); /* Brabble a bit about the parsing/decoding. */

  /* Now mpg123 is being prepared for feeding. The main loop will read chunks from stdin and feed them to mpg123;
     then take decoded data as available to write to stdout. */
  mpg123_open_feed(m);

  while(1) /* Read and write until everything is through. */
  {
next_packet:
    if (is_paused) {
      // Wait for resume message
      //fprintf(stderr,"acodec: Waiting for resume\n");
      pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
      is_paused = 0;
      pthread_mutex_unlock(&codec->queue_mutex);
    }
    current = codec_queue_get_next_item(codec);

    if (current->msgtype == MSG_STOP) {
      DEBUGF("[acodec_mpeg] Stopping\n");
      codec_queue_free_item(codec,current);
      goto stop;
    } else if (current->msgtype == MSG_PAUSE) {
      //fprintf(stderr,"acodec: Paused\n");
      codec_queue_free_item(codec,current);
      current = NULL;
      is_paused = 1;
      goto next_packet;
    }


    buf = get_next_buffer(&pipe->audio_render);
    buf->nTimeStamp = pts_to_omx(current->data->PTS);
    buf->nFlags = 0;
    if(first_packet)
      {
	buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
	first_packet = 0;
      }
    buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;


    /* Feed input chunk and get first chunk of decoded audio. */
    ret = mpg123_decode(m,current->data->packet,current->data->packetlength,buf->pBuffer,buf->nAllocLen,&buf->nFilledLen);

    if(ret == MPG123_NEW_FORMAT)
    {
      long rate;
      int channels, enc;
      mpg123_getformat(m, &rate, &channels, &enc);
      DEBUGF("New format: %li Hz, %i channels, encoding value %i\n", rate, channels, enc);

      assert(ret == 0);
    }
#if 0
    while(ret != MPG123_ERR && ret != MPG123_NEED_MORE)
    { /* Get all decoded audio that is available now before feeding more input. */
      ret = mpg123_decode(m,NULL,0,out,OUTBUFF,&size);
      output_pcm(st, out, size, buffer_size);
      outc += size;
    }
    if(ret == MPG123_ERR){ fprintf(stderr, "some error: %s", mpg123_strerror(m)); break; }
#endif

    OERR(OMX_EmptyThisBuffer(pipe->audio_render.h, buf));

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:
  /* Done decoding, now just clean up and leave. */
  mpg123_delete(m);
  mpg123_exit();

  return 0;
}

void acodec_mpeg_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_mpeg_thread,(void*)args);
}
