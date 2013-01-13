
/* MPEG audio codec using libmpg123.  Based on mpglib.c sample from libmpg123

   Audio code taken from the hello_audio.c sample
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
#include "audioplay.h"
#include "debug.h"

#define INBUFF  16384
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

static void* acodec_mpeg_thread(struct codec_t* codec)
{
  struct codec_queue_t* current = NULL;
  size_t size;
  unsigned char out[OUTBUFF]; /* output buffer */
  size_t outc = 0;
  mpg123_handle *m;
  int ret;
  int is_paused = 0;

  int nchannels = 2;        // numnber of audio channels
  int bitdepth = 16;       // number of bits per sample

  AUDIOPLAY_STATE_T *st;
  int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * nchannels)>>3;

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

    /* Feed input chunk and get first chunk of decoded audio. */
    ret = mpg123_decode(m,current->data->packet,current->data->packetlength,out,OUTBUFF,&size);

    if(ret == MPG123_NEW_FORMAT)
    {
      long rate;
      int channels, enc;
      mpg123_getformat(m, &rate, &channels, &enc);
      DEBUGF("New format: %li Hz, %i channels, encoding value %i\n", rate, channels, enc);

      ret = audioplay_create(&st, rate, channels, bitdepth, 10, buffer_size);
      assert(ret == 0);
    }

    output_pcm(st, out, size, buffer_size);

    while(ret != MPG123_ERR && ret != MPG123_NEED_MORE)
    { /* Get all decoded audio that is available now before feeding more input. */
      ret = mpg123_decode(m,NULL,0,out,OUTBUFF,&size);
      output_pcm(st, out, size, buffer_size);
      outc += size;
    }
    if(ret == MPG123_ERR){ fprintf(stderr, "some error: %s", mpg123_strerror(m)); break; }

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:
  audioplay_delete(st);

  /* Done decoding, now just clean up and leave. */
  mpg123_delete(m);
  mpg123_exit();

  return 0;
}

void acodec_mpeg_init(struct codec_t* codec)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_mpeg_thread,(void*)codec);
}
