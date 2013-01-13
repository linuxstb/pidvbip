/*

audioplay_* functions taken from "hello_audio.c" sample program 

Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Audio output demo using OpenMAX IL though the ilcient helper library

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <semaphore.h>

#include "audioplay.h"

// Audio destination - "local" or "hdmi" (yes, these are strings)
#define AUDIO_DEST "hdmi"

static void input_buffer_callback(void *data, COMPONENT_T *comp)
{
   // do nothing - could add a callback to the user
   // to indicate more buffers may be available.
}

int32_t audioplay_create(AUDIOPLAY_STATE_T **handle,
                         uint32_t sample_rate,
                         uint32_t num_channels,
                         uint32_t bit_depth,
                         uint32_t num_buffers,
                         uint32_t buffer_size)
{
   uint32_t bytes_per_sample = (bit_depth * num_channels) >> 3;
   int32_t ret = -1;

   *handle = NULL;

   // basic sanity check on arguments
   if(sample_rate >= 8000 && sample_rate <= 96000 &&
      (num_channels == 1 || num_channels == 2 || num_channels == 4 || num_channels == 8) &&
      (bit_depth == 16 || bit_depth == 32) &&
      num_buffers > 0 &&
      buffer_size >= bytes_per_sample)
   {
      // buffer lengths must be 16 byte aligned for VCHI
      int size = (buffer_size + 15) & ~15;
      AUDIOPLAY_STATE_T *st;

      // buffer offsets must also be 16 byte aligned for VCHI
      st = calloc(1, sizeof(AUDIOPLAY_STATE_T));

      if(st)
      {
         OMX_ERRORTYPE error;
         OMX_PARAM_PORTDEFINITIONTYPE param;
         OMX_AUDIO_PARAM_PCMMODETYPE pcm;
         int32_t s;

         ret = 0;
         *handle = st;

         // create and start up everything
         s = sem_init(&st->sema, 0, 1);
         assert(s == 0);

         st->bytes_per_sample = bytes_per_sample;
         st->num_buffers = num_buffers;

         st->client = ilclient_init();
         assert(st->client != NULL);

         ilclient_set_empty_buffer_done_callback(st->client, input_buffer_callback, st);

         error = OMX_Init();
         assert(error == OMX_ErrorNone);

         ilclient_create_component(st->client, &st->audio_render, "audio_render", ILCLIENT_ENABLE_INPUT_BUFFERS | ILCLIENT_DISABLE_ALL_PORTS);
         assert(st->audio_render != NULL);

         st->list[0] = st->audio_render;

         // set up the number/size of buffers
         memset(&param, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
         param.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
         param.nVersion.nVersion = OMX_VERSION;
         param.nPortIndex = 100;

         error = OMX_GetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamPortDefinition, &param);
         assert(error == OMX_ErrorNone);

         param.nBufferSize = size;
         param.nBufferCountActual = num_buffers;

         error = OMX_SetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamPortDefinition, &param);
         assert(error == OMX_ErrorNone);

         // set the pcm parameters
         memset(&pcm, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
         pcm.nSize = sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
         pcm.nVersion.nVersion = OMX_VERSION;
         pcm.nPortIndex = 100;
         pcm.nChannels = num_channels;
         pcm.eNumData = OMX_NumericalDataSigned;
         pcm.eEndian = OMX_EndianLittle;
         pcm.nSamplingRate = sample_rate;
         pcm.bInterleaved = OMX_TRUE;
         pcm.nBitPerSample = bit_depth;
         pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;

         switch(num_channels) {
         case 1:
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelCF;
            break;
         case 8:
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcm.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
            pcm.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
            pcm.eChannelMapping[4] = OMX_AUDIO_ChannelLR;
            pcm.eChannelMapping[5] = OMX_AUDIO_ChannelRR;
            pcm.eChannelMapping[6] = OMX_AUDIO_ChannelLS;
            pcm.eChannelMapping[7] = OMX_AUDIO_ChannelRS;
            break;
         case 4:
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            pcm.eChannelMapping[2] = OMX_AUDIO_ChannelLR;
            pcm.eChannelMapping[3] = OMX_AUDIO_ChannelRR;
            break;
         case 2:
            pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
            break;
         }

         error = OMX_SetParameter(ILC_GET_HANDLE(st->audio_render), OMX_IndexParamAudioPcm, &pcm);
         assert(error == OMX_ErrorNone);

         ilclient_change_component_state(st->audio_render, OMX_StateIdle);
         if(ilclient_enable_port_buffers(st->audio_render, 100, NULL, NULL, NULL) < 0)
         {
            // error
            ilclient_change_component_state(st->audio_render, OMX_StateLoaded);
            ilclient_cleanup_components(st->list);

            error = OMX_Deinit();
            assert(error == OMX_ErrorNone);

            ilclient_destroy(st->client);

            sem_destroy(&st->sema);
            free(st);
            *handle = NULL;
            return -1;
         }

         ilclient_change_component_state(st->audio_render, OMX_StateExecuting);

         audioplay_set_dest(st, AUDIO_DEST);
      }
   }

   return ret;
}

int32_t audioplay_delete(AUDIOPLAY_STATE_T *st)
{
   OMX_ERRORTYPE error;

   ilclient_change_component_state(st->audio_render, OMX_StateIdle);

   error = OMX_SendCommand(ILC_GET_HANDLE(st->audio_render), OMX_CommandStateSet, OMX_StateLoaded, NULL);
   assert(error == OMX_ErrorNone);

   ilclient_disable_port_buffers(st->audio_render, 100, st->user_buffer_list, NULL, NULL);
   ilclient_change_component_state(st->audio_render, OMX_StateLoaded);
   ilclient_cleanup_components(st->list);

   error = OMX_Deinit();
   assert(error == OMX_ErrorNone);

   ilclient_destroy(st->client);

   sem_destroy(&st->sema);
   free(st);

   return 0;
}

uint8_t *audioplay_get_buffer(AUDIOPLAY_STATE_T *st)
{
   OMX_BUFFERHEADERTYPE *hdr = NULL;

   hdr = ilclient_get_input_buffer(st->audio_render, 100, 0);

   if(hdr)
   {
      // put on the user list
      sem_wait(&st->sema);

      hdr->pAppPrivate = st->user_buffer_list;
      st->user_buffer_list = hdr;

      sem_post(&st->sema);
   }

   return hdr ? hdr->pBuffer : NULL;
}

int32_t audioplay_play_buffer(AUDIOPLAY_STATE_T *st,
                              uint8_t *buffer,
                              uint32_t length)
{
   OMX_BUFFERHEADERTYPE *hdr = NULL, *prev = NULL;
   int32_t ret = -1;

   if(length % st->bytes_per_sample)
      return ret;

   sem_wait(&st->sema);

   // search through user list for the right buffer header
   hdr = st->user_buffer_list;
   while(hdr != NULL && hdr->pBuffer != buffer && hdr->nAllocLen < length)
   {
      prev = hdr;
      hdr = hdr->pAppPrivate;
   }

   if(hdr) // we found it, remove from list
   {
      ret = 0;
      if(prev)
         prev->pAppPrivate = hdr->pAppPrivate;
      else
         st->user_buffer_list = hdr->pAppPrivate;
   }

   sem_post(&st->sema);

   if(hdr)
   {
      OMX_ERRORTYPE error;

      hdr->pAppPrivate = NULL;
      hdr->nOffset = 0;
      hdr->nFilledLen = length;

      error = OMX_EmptyThisBuffer(ILC_GET_HANDLE(st->audio_render), hdr);
      assert(error == OMX_ErrorNone);
   }

   return ret;
}

int32_t audioplay_set_dest(AUDIOPLAY_STATE_T *st, const char *name)
{
   int32_t success = -1;
   OMX_CONFIG_BRCMAUDIODESTINATIONTYPE ar_dest;

   if (name && strlen(name) < sizeof(ar_dest.sName))
   {
      OMX_ERRORTYPE error;
      memset(&ar_dest, 0, sizeof(ar_dest));
      ar_dest.nSize = sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE);
      ar_dest.nVersion.nVersion = OMX_VERSION;
      strcpy((char *)ar_dest.sName, name);

      error = OMX_SetConfig(ILC_GET_HANDLE(st->audio_render), OMX_IndexConfigBrcmAudioDestination, &ar_dest);
      assert(error == OMX_ErrorNone);
      success = 0;
   }

   return success;
}


uint32_t audioplay_get_latency(AUDIOPLAY_STATE_T *st)
{
   OMX_PARAM_U32TYPE param;
   OMX_ERRORTYPE error;

   memset(&param, 0, sizeof(OMX_PARAM_U32TYPE));
   param.nSize = sizeof(OMX_PARAM_U32TYPE);
   param.nVersion.nVersion = OMX_VERSION;
   param.nPortIndex = 100;

   error = OMX_GetConfig(ILC_GET_HANDLE(st->audio_render), OMX_IndexConfigAudioRenderingLatency, &param);
   assert(error == OMX_ErrorNone);

   return param.nU32;
}

