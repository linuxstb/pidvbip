
/* A52 audio codec using liba52
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <a52dec/a52.h>
#include "codec.h"
#include "acodec_a52.h"
#include "omx_utils.h"
#include "debug.h"

static void* acodec_a52_thread(struct codec_init_args_t* args)
{
  struct codec_t* codec = args->codec;
  struct omx_pipeline_t* pipe = args->pipe;
  OMX_PARAM_PORTDEFINITIONTYPE param;
  struct codec_queue_t* current = NULL;
  size_t size;
  int ret;
  a52_state_t *state;
  unsigned long samplesdone;
  unsigned long frequency;
  int flags;
  int sample_rate;
  int bit_rate;
  int is_paused = 0;
  OMX_BUFFERHEADERTYPE *buf;
  int first_packet = 1;

  free(args);

  /* Intialise the A52 decoder and check for success */
  state = a52_init(0);

  while(1)
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

    //    a52_decode_data(current->data->packet,current->data->packet + current->data->packetlength, st, buffer_size);
    int length;
    length = a52_syncinfo(current->data->packet, &flags, &sample_rate, &bit_rate);

    if (length != current->data->packetlength) {
      fprintf(stderr,"[acodec_a52] length (%d) != packetlength (%d)\n",length,current->data->packetlength);
      goto error;
    }

    //fprintf(stderr,"length=%d, flags=%d, sample_rate=%d, bit_rate=%d, packetlength=%d                 \n",length,flags,sample_rate,bit_rate,current->data->packetlength);

    sample_t level = 1 << 15;
    sample_t bias=0;
    int i;
    int gain = 1; // Gain in dB

    level *= gain;

    /* This is the configuration for the downmixing: */
    flags = A52_STEREO | A52_ADJUST_LEVEL;

    if (a52_frame (state, current->data->packet, &flags, &level, bias)) {
      fprintf(stderr,"error in a52_frame()\n");
      goto error;
    }

    a52_dynrng (state, NULL, NULL);

    buf = get_next_buffer(&pipe->audio_render);
    buf->nFilledLen = 2 * 2 * 6 * 256;
    buf->nTimeStamp = pts_to_omx(current->data->PTS);

    buf->nFlags = 0;
    if(first_packet)
    {
      buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
      first_packet = 0;
    }

    buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

    int16_t* p = (int16_t*)buf->pBuffer;

    for (i = 0; i < 6; i++) {
      if (a52_block (state)) {
        fprintf(stderr,"error in a52_block() - i=%d\n",i);
        goto error;
      }

      /* Convert samples to output format */
//      int16_t data[512];
//      int16_t* p = data;
      int j;
      sample_t* samples = a52_samples(state);

      for (j=0;j<256;j++) {
        *p++ = samples[j];
        *p++ = samples[j+256];
      }
      //output_pcm(st, (unsigned char*)data, sizeof(data), buffer_size);
    }

    OERR(OMX_EmptyThisBuffer(pipe->audio_render.h, buf));
error:

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:

  return 0;
}

void acodec_a52_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_a52_thread,(void*)args);
}
