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
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <a52dec/a52.h>
#include <mpg123.h>
#include <neaacdec.h>
#include "codec.h"
#include "acodec_omx.h"
#include "omx_utils.h"
#include "htsp.h"  /* For HMF_AUDIO_* defines - OpenMax isn't good enough */
#include "debug.h"

static int decode_packet_a52( a52_state_t *state, OMX_BUFFERHEADERTYPE *buf, struct packet_t* data)
{
  int length;
  int flags;
  int sample_rate;
  int bit_rate;

  buf->nFilledLen = 2 * 2 * 6 * 256;
  length = a52_syncinfo(data->packet, &flags, &sample_rate, &bit_rate);

  if (length != data->packetlength) {
    fprintf(stderr,"[acodec_a52] length (%d) != packetlength (%d)\n",length,data->packetlength);
    return -1;
  }

  //fprintf(stderr,"length=%d, flags=%d, sample_rate=%d, bit_rate=%d, packetlength=%d                 \n",length,flags,sample_rate,bit_rate,data->packetlength);

  sample_t level = 1 << 15;
  sample_t bias=0;
  int i;
  int gain = 1; // Gain in dB

  level *= gain;

  /* This is the configuration for the downmixing: */
  flags = A52_STEREO | A52_ADJUST_LEVEL;

  if (a52_frame (state, data->packet, &flags, &level, bias)) {
    fprintf(stderr,"error in a52_frame()\n");
    return -1;
  }

  a52_dynrng (state, NULL, NULL);

  int16_t* p = (int16_t*)buf->pBuffer;

  for (i = 0; i < 6; i++) {
    if (a52_block (state)) {
      fprintf(stderr,"error in a52_block() - i=%d\n",i);
      return -1;
    }

    /* Convert samples to output format */
    int j;
    sample_t* samples = a52_samples(state);

    for (j=0;j<256;j++) {
      *p++ = samples[j];
      *p++ = samples[j+256];
    }
  }

  return 0;
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


static void* acodec_omx_thread(struct codec_init_args_t* args)
{
  struct codec_t* codec = args->codec;
  struct omx_pipeline_t* pipe = args->pipe;
  OMX_PARAM_PORTDEFINITIONTYPE param;
  struct codec_queue_t* current = NULL;
  size_t size;
  int res;
  a52_state_t *state;
  mpg123_handle *m;
  unsigned long samplesdone;
  unsigned long frequency;
  int is_paused = 0;
  OMX_BUFFERHEADERTYPE *buf;
  long unsigned int aac_samplerate = 48000;  // audio sample rate in Hz
  unsigned char aac_channels = NCHANNELS;        // numnber of audio channels

  free(args);

  /***** Initialise the A52 decoder *****/
  state = a52_init(0);

  /***** Initialise MPEG audio decoder *****/
  mpg123_init();

  m = mpg123_new(NULL, &res);

  if(m == NULL)
  {
    fprintf(stderr,"Unable to create mpg123 handle: %s\n", mpg123_plain_strerror(res));
    exit(1);
  }
  mpg123_param(m, MPG123_VERBOSE, 2, 0); /* Brabble a bit about the parsing/decoding. */
  mpg123_param(m, MPG123_FLAGS, MPG123_FORCE_STEREO, 0); /* Force Stereo output */

  /* Now mpg123 is being prepared for feeding. The main loop will read chunks from stdin and feed them to mpg123;
     then take decoded data as available to write to stdout. */
  mpg123_open_feed(m);



  /***** Initialise AAC decoder *****/
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

new_channel:
  while(1)
  {
next_packet:
    /* NOTE: This lock is only used by the video thread when setting
       up or tearing down the pipeline, so we are not blocking normal
       video playback 
    */
    //fprintf(stderr,"[acodec] - waiting for omx_active_mutex\n");
    pthread_mutex_lock(&pipe->omx_active_mutex);
    //fprintf(stderr,"[acodec] - got omx_active_mutex\n");
    while (!pipe->omx_active) {
      pthread_cond_wait(&pipe->omx_active_cv, &pipe->omx_active_mutex);
      //fprintf(stderr,"[acodec] - omx_active=%d\n",pipe->omx_active);
    }

    if (is_paused) {
      // Wait for resume message
      //fprintf(stderr,"acodec: Waiting for resume\n");
      pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
      is_paused = 0;
      pthread_mutex_unlock(&codec->queue_mutex);
    }
    current = codec_queue_get_next_item(codec);

    if (current->msgtype == MSG_STOP) {
      DEBUGF("[acodec] Stopping\n");
      codec_queue_free_item(codec,current);
      pthread_mutex_unlock(&pipe->omx_active_mutex);
      goto stop;
    } else if (current->msgtype == MSG_NEW_CHANNEL) {
      //fprintf(stderr,"[acodec] NEW_CHANNEL received, going to new_channel\n");
      codec_queue_free_item(codec,current);
      pthread_mutex_unlock(&pipe->omx_active_mutex);
      goto new_channel;;
    } else if (current->msgtype == MSG_PAUSE) {
      //fprintf(stderr,"acodec: Paused\n");
      codec_queue_free_item(codec,current);
      is_paused = 1;
      pthread_mutex_unlock(&pipe->omx_active_mutex);
      goto next_packet;
    }

    buf = get_next_buffer(&pipe->audio_render);
    buf->nTimeStamp = pts_to_omx(current->data->PTS);
    //fprintf(stderr,"Audio timestamp=%lld\n",current->data->PTS);

    res = -1;
    if (codec->acodectype == HMF_AUDIO_CODEC_AC3) {
      res = decode_packet_a52(state, buf, current->data);
    } else if (codec->acodectype == HMF_AUDIO_CODEC_MPEG) {
      res = mpg123_decode(m,current->data->packet,current->data->packetlength,buf->pBuffer,buf->nAllocLen,&buf->nFilledLen);
      res = (res == MPG123_ERR);
    } else if (codec->acodectype == HMF_AUDIO_CODEC_AAC) {
      if (!done_init) {
        int err = NeAACDecInit2(hAac, codec->codecdata, codec->codecdatasize, &aac_samplerate, &aac_channels);
        //err = NeAACDecInit(hAac, current->data->packet, 7, &samplerate, &nchannels);
        if (err) {
          fprintf(stderr,"NeAACDecInit2 - error %d\n",err);
          exit(1);
        }
  
        done_init = 1;
      }
      int header_length = 0;
      unsigned char* s = current->data->packet;
      if ((s[0] == 0xff) && ((s[0] & 0xf0) == 0xf0)) { /* ADTS 7 or 9 byte header */
        if (s[1] & 1) { // CRC absent
          header_length = 7;
        } else {
          header_length = 9;
        }
      }
      NeAACDecFrameInfo hInfo;
      unsigned char* ret = NeAACDecDecode(hAac, &hInfo, current->data->packet + header_length, current->data->packetlength-header_length);
  
      if (hInfo.error) {
        fprintf(stderr,"Error decoding frame - %d (%s)\n",hInfo.error,NeAACDecGetErrorMessage(hInfo.error));
        exit(1);
      }
  
      if (hInfo.bytesconsumed != current->data->packetlength - header_length) {
        fprintf(stderr,"AUDIO: Did not consume entire packet\n");
        exit(1);
      }
  
      if (hInfo.samples > 0) {
        memcpy(buf->pBuffer, ret, hInfo.samples * 2);
        buf->nFilledLen = hInfo.samples * 2;
        res = 0;
      }
    }

    if (res == 0) {
      buf->nFlags = 0;
      if(codec->first_packet)
      {
        //usleep(1000000);
        fprintf(stderr,"First audio packet\n");
        buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        codec->first_packet = 0;
      }

      buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

      OERR(OMX_EmptyThisBuffer(pipe->audio_render.h, buf));
    }

    pthread_mutex_unlock(&pipe->omx_active_mutex);

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:

  /* Done decoding, now just clean up and leave. */
  mpg123_delete(m);
  mpg123_exit();

  return 0;
}

void acodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_omx_thread,(void*)args);
}
