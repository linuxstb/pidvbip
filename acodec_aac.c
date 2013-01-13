
/* AAC audio codec.

   Audio code taken from the hello_audio.c sample
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <neaacdec.h>
#include "codec.h"
#include "acodec_aac.h"
#include "audioplay.h"

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

/*

From http://wiki.multimedia.cx/index.php?title=Understanding_AAC

5 bits: object type
4 bits: frequency index
if (frequency index == 15)
    24 bits: frequency
4 bits: channel configuration
1 bit: frame length flag
1 bit: dependsOnCoreCoder
1 bit: extensionFlag

*/

#define NCHANNELS 2

static void* create_aac_codecdata(struct codec_t* codec)
{
  int object_type = 2;        /* 2 = LC, see http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio */
  int frequency_index = 3;    /* 3 = 48000, see http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio */
  int channel_config = NCHANNELS;     /* 2 = Stereo, see http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio */
  int frame_length = 0;       /* 0 = 1024 samples, 1 = 960 samples */
  int dependsOnCoreCoder = 0;
  int extensionFlag = 0;

  codec->codecdata = malloc(2);
  codec->codecdatasize = 2;

  codec->codecdata[0] = (object_type << 3) | ((frequency_index & 0xe) >> 1);
  codec->codecdata[1] = ((frequency_index & 0x01) << 7) | (channel_config << 3) | (frame_length << 2) | (dependsOnCoreCoder << 1) | extensionFlag;

  fprintf(stderr,"Codecdata created = 0x%02x 0x%02x\n",codec->codecdata[0],codec->codecdata[1]);
}

static void* acodec_aac_thread(struct codec_t* codec)
{
  struct codec_queue_t* current = NULL;
  size_t size;
  int ret;
  int is_paused = 0;

  long unsigned int samplerate = 48000;  // audio sample rate in Hz
  unsigned char nchannels = NCHANNELS;        // numnber of audio channels
  int bitdepth = 16;       // number of bits per sample

  AUDIOPLAY_STATE_T *st;
  int buffer_size = (BUFFER_SIZE_SAMPLES * bitdepth * nchannels)>>3;
  int err;

  ret = audioplay_create(&st, samplerate, nchannels, bitdepth, 10, buffer_size);
  assert(ret == 0);

  // Check if decoder has the needed capabilities
  //unsigned long cap = NeAACDecGetCapabilities();

  // Open the library
  NeAACDecHandle hAac = NeAACDecOpen();
  if (!hAac) {
    fprintf(stderr,"Could not open FAAD decoder\n");
    exit(1);
  }


  // Get the current config
  NeAACDecConfigurationPtr conf =  NeAACDecGetCurrentConfiguration(hAac);

  conf->outputFormat = FAAD_FMT_16BIT;
  conf->downMatrix = 1;

  // Set the new configuration
  NeAACDecSetConfiguration(hAac, conf);

  // Initialise the library using one of the initialization functions
  create_aac_codecdata(codec);
  
  int done_init = 0;

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
      codec_queue_free_item(codec,current);
      goto stop;
    } else if (current->msgtype == MSG_PAUSE) {
      //fprintf(stderr,"acodec: Paused\n");
      codec_queue_free_item(codec,current);
      current = NULL;
      is_paused = 1;
      goto next_packet;
    }

    if (!done_init) {
      err = NeAACDecInit2(hAac, codec->codecdata, codec->codecdatasize, &samplerate, &nchannels);
      //err = NeAACDecInit(hAac, current->data->packet, 7, &samplerate, &nchannels);
      if (err) {
        fprintf(stderr,"NeAACDecInit2 - error %d\n",err);
        exit(1);
      }

      done_init = 1;
    }
    NeAACDecFrameInfo hInfo;
//fprintf(stderr,"Decoding packet of length %d\n",current->data->packetlength);
    unsigned char* ret = NeAACDecDecode(hAac, &hInfo, current->data->packet + 7, current->data->packetlength-7);

    if ((hInfo.error == 0) && (hInfo.samples > 0)) {
	//
	// do what you need to do with the decoded samples
	//
        //fprintf(stderr,"Outputting %d samples\n",hInfo.samples);
    //fprintf(stderr,"bytes_consumed = %d\n",(int)hInfo.bytesconsumed);
        if (hInfo.bytesconsumed != current->data->packetlength - 7) {
          fprintf(stderr,"Did not consume entire packet\n");
          exit(1);
        }

        output_pcm(st, ret, hInfo.samples * 2, buffer_size);
    } else if (hInfo.error != 0) {
      //
      // Some error occurred while decoding this frame
      //
      fprintf(stderr,"Error decoding frame - %d\n",hInfo.error);
      exit(1);
    }

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:
  audioplay_delete(st);

  /* Done decoding, now just clean up and leave. */

  return 0;
}

void acodec_aac_init(struct codec_t* codec)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_aac_thread,(void*)codec);
}
