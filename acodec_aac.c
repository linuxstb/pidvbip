
/* AAC audio codec.

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
#include "omx_utils.h"

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

static void* acodec_aac_thread(struct codec_init_args_t* args)
{
  struct codec_t* codec = args->codec;
  struct omx_pipeline_t* pipe = args->pipe;
  struct codec_queue_t* current = NULL;
  size_t size;
  int ret;
  int is_paused = 0;
  OMX_BUFFERHEADERTYPE *buf;
  int first_packet = 1;

  long unsigned int samplerate = 48000;  // audio sample rate in Hz
  unsigned char nchannels = NCHANNELS;        // numnber of audio channels
  int bitdepth = 16;       // number of bits per sample

  free(args);

  int err;

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

    if (hInfo.error) {
      fprintf(stderr,"Error decoding frame - %d\n",hInfo.error);
      exit(1);
    }

    if (hInfo.bytesconsumed != current->data->packetlength - 7) {
      fprintf(stderr,"Did not consume entire packet\n");
      exit(1);
    }

    //output_pcm(st, ret, hInfo.samples * 2, buffer_size);

    if (hInfo.samples > 0) {
      buf = get_next_buffer(&pipe->audio_render);
      memcpy(buf->pBuffer, ret, hInfo.samples * 2);
      buf->nFilledLen = hInfo.samples * 2;
      buf->nTimeStamp = pts_to_omx(current->data->PTS);

      buf->nFlags = 0;
      if (first_packet) {
        buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        first_packet = 0;
      }

      buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

      OERR(OMX_EmptyThisBuffer(pipe->audio_render.h, buf));
    }

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }
stop:
  /* Disable audio_render input port and buffers */
  omx_send_command_and_wait0(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);
  omx_free_buffers(&pipe->audio_render, 100);
  omx_send_command_and_wait1(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);

  /* Transition to StateLoaded and free the handle */
  omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
  omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
  OERR(OMX_FreeHandle(pipe->audio_render.h));

  /* Done decoding, now just clean up and leave. */

  return 0;
}

void acodec_aac_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->codecstate = NULL;

  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_aac_thread,(void*)args);
}
