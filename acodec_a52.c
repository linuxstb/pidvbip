
/* A52 audio codec using liba52

   Audio code taken from the hello_audio.c sample
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
#include "audioplay.h"
#include "debug.h"

#define OUTBUFF 32768 

#define CTTW_SLEEP_TIME 10
#define MIN_LATENCY_TIME 20

#define BUFFER_SIZE_SAMPLES 1152

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static void output_pcm(AUDIOPLAY_STATE_T *st,unsigned char* data, int size, int buffer_size)
{
  uint8_t *buf;
  int copied = 0;
  int latency;
  int samplerate = 48000;  // audio sample rate in Hz
  int ret;

  while (size > 0) 
  {
    while((buf = audioplay_get_buffer(st)) == NULL)
      usleep(10*1000);

    int to_copy = MIN(size,buffer_size);

    memcpy(buf,data + copied,to_copy);
    copied += to_copy;
    size -= to_copy;

    // try and wait for a minimum latency time (in ms) before
    // sending the next packet
    while((latency = audioplay_get_latency(st)) > (samplerate * (MIN_LATENCY_TIME + CTTW_SLEEP_TIME) / 1000))
       usleep(CTTW_SLEEP_TIME*1000);

    ret = audioplay_play_buffer(st, buf, to_copy);

    if (ret != 0) {
      fprintf(stderr,"[acodec_mpeg] - audioplay_play_buffer() error %d\n",ret);
      exit(1);
    }
  }
}

#define BUFFER_SIZE 4096

#define A52_SAMPLESPERFRAME (6*256)

static void* acodec_a52_thread(struct codec_t* codec)
{
  struct codec_queue_t* current = NULL;
  size_t size;
  unsigned char out[OUTBUFF]; /* output buffer */
  size_t outc = 0;
  int ret;
  a52_state_t *state;
  unsigned long samplesdone;
  unsigned long frequency;
  int flags;
  int sample_rate;
  int bit_rate;
  int is_paused = 0;

  int nchannels = 2;        // numnber of audio channels
  int bitdepth = 16;       // number of bits per sample

  AUDIOPLAY_STATE_T *st;
  int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * nchannels)>>3;

  /* Intialise the A52 decoder and check for success */
  state = a52_init(0);

  /* Initialise audio output - hardcoded to 48000/Stereo/16-bit */

  ret = audioplay_create(&st, 48000, 2, 16, 10, buffer_size);
  assert(ret == 0);

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

    for (i = 0; i < 6; i++) {
      if (a52_block (state)) {
        fprintf(stderr,"error in a52_block() - i=%d\n",i);
        goto error;
      }

      /* Convert samples to output format */
      int16_t data[512];
      int16_t* p = data;
      int j;
      sample_t* samples = a52_samples(state);

      for (j=0;j<256;j++) {
        *p++ = samples[j];
        *p++ = samples[j+256];
      }
      output_pcm(st, (unsigned char*)data, sizeof(data), buffer_size);
    }
error:

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:
  audioplay_delete(st);

  /* Done decoding, now just clean up and leave. */

  return 0;
}

void acodec_a52_init(struct codec_t* codec)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_a52_thread,(void*)codec);
}
