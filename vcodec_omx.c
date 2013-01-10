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


This file was based on the hello_video.c sample program which had the
following (C):


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

// Video deocode demo using OpenMAX IL though the ilcient helper library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bcm_host.h>
#include "libs/ilclient/ilclient.h"

#include "vcodec_omx.h"
#include "codec.h"
#include "debug.h"

static void* vcodec_omx_thread(struct codec_t* codec)
{
   struct codec_queue_t* current = NULL;
   int current_used = 0;
   OMX_VIDEO_PARAM_PORTFORMATTYPE format;
   OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
   COMPONENT_T *video_decode = NULL, *video_scheduler = NULL, *video_render = NULL, *clock = NULL;
   COMPONENT_T *list[5];
   TUNNEL_T tunnel[4];
   ILCLIENT_T *client;
   int status = 0;
   unsigned char *data = NULL;
   unsigned int data_len = 0;
   int packet_size = 16<<10; // NOTE: 16KB
   int frames_sent = 0;
   int is_paused = 0;
   int64_t prev_DTS = -1;

   memset(list, 0, sizeof(list));
   memset(tunnel, 0, sizeof(tunnel));

   if((client = ilclient_init()) == NULL)
   {
      fprintf(stderr,"Could not init ilclient\n");
      exit(1);
      return -3;
   }

   if(OMX_Init() != OMX_ErrorNone)
   {
      ilclient_destroy(client);
      fprintf(stderr,"Could not init OMX\n");
      exit(1);
      return -4;
   }

   // create video_decode
   if(ilclient_create_component(client, &video_decode, "video_decode", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0)
      status = -14;
   list[0] = video_decode;

   // create video_render
   if(status == 0 && ilclient_create_component(client, &video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[1] = video_render;

   // create clock
   if(status == 0 && ilclient_create_component(client, &clock, "clock", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[2] = clock;

   memset(&cstate, 0, sizeof(cstate));
   cstate.nSize = sizeof(cstate);
   cstate.nVersion.nVersion = OMX_VERSION;
   cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
   cstate.nWaitMask = 1;
   if(clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
      status = -13;

   // create video_scheduler
   if(status == 0 && ilclient_create_component(client, &video_scheduler, "video_scheduler", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[3] = video_scheduler;

   set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
   set_tunnel(tunnel+1, video_scheduler, 11, video_render, 90);
   set_tunnel(tunnel+2, clock, 80, video_scheduler, 12);

   // setup clock tunnel first
   if(status == 0 && ilclient_setup_tunnel(tunnel+2, 0, 0) != 0)
      status = -15;
   else
      ilclient_change_component_state(clock, OMX_StateExecuting);

   if(status == 0)
      ilclient_change_component_state(video_decode, OMX_StateIdle);

   memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
   format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
   format.nVersion.nVersion = OMX_VERSION;
   format.nPortIndex = 130;
   format.eCompressionFormat = codec->codectype;

   if(status == 0 &&
      OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone &&
      ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) == 0)
   {
      OMX_BUFFERHEADERTYPE *buf;
      int port_settings_changed = 0;
      int first_packet = 1;

      /* Enable error concealment for H264 only - without this, HD channels don't work reliably */
      if (codec->codectype == OMX_VIDEO_CodingAVC) {
        OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE ec;
        memset (&ec, 0, sizeof ec);
        ec.nSize = sizeof ec;
        ec.nVersion.nVersion = OMX_VERSION;
        ec.bStartWithValidFrame = OMX_FALSE;
        OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamBrcmVideoDecodeErrorConcealment, &ec);
      }

      ilclient_change_component_state(video_decode, OMX_StateExecuting);

      while((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL)
      {
         // feed data and wait until we get port settings changed
         unsigned char *dest = buf->pBuffer;

#if 0
// Original code:
         data_len += fread(dest, 1, packet_size+(find_start_codes*4)-data_len, in);
#endif

next_packet:
         /******* Start of new code for vcodec_omx.c */
         if (current == NULL) {
           if (is_paused) {
	     // Wait for resume message
             //fprintf(stderr,"vcodec: Waiting for resume\n");
             pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
             pthread_mutex_unlock(&codec->queue_mutex);
             is_paused = 0;
           }
           current = codec_queue_get_next_item(codec); 
           current_used = 0; 

           if (current->msgtype == MSG_STOP) {
             codec_queue_free_item(codec,current);
             current = NULL;
             fprintf(stderr,"\nframes_sent=%d\n",frames_sent);
             goto stop;
           } else if (current->msgtype == MSG_PAUSE) {
             //fprintf(stderr,"vcodec: Paused\n");
             codec_queue_free_item(codec,current);
             current = NULL;
             is_paused = 1;
             goto next_packet;
           }

           /* Simple implementation of A/V sync - sync video DTS with audio PTS.  
              The proper way is to use OMX clocks */
           int64_t audio_latency = 70000; /* 70ms - a guess which seems to work */
           int64_t audio_PTS = codec_get_pts(codec->acodec);
           if ((prev_DTS != -1) && ((prev_DTS + 40000) != current->data->DTS) && ((prev_DTS + 20000) != current->data->DTS)) {
             fprintf(stderr,"DTS discontinuity - DTS=%lld, prev_DTS=%lld (diff = %lld)\n",current->data->DTS,prev_DTS,current->data->DTS-prev_DTS);
           }
           prev_DTS = current->data->DTS;
           if (audio_PTS != -1) {
             int64_t delay = current->data->DTS-(audio_PTS+audio_latency);
             if (delay > 50000) delay = 50000;
             if (delay > 0) {
               //fprintf(stderr,"[vcodec_omx] udelay(%lld)\n",delay);
               usleep(delay);
             }
           }
         }

         int to_copy = packet_size - data_len;

         if (to_copy > (current->data->packetlength - current_used))
           to_copy = current->data->packetlength - current_used;

         memcpy(dest,current->data->packet + current_used,to_copy);
         data_len += to_copy;
         current_used += to_copy;

         if (current_used == current->data->packetlength)
         {
           codec_queue_free_item(codec,current);
           current = NULL;
         }
         frames_sent++;
         /******* End of new code for vcodec_omx.c */

         if(port_settings_changed == 0 &&
            ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
             (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,
                                                       ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
         {
            port_settings_changed = 1;

            if(ilclient_setup_tunnel(tunnel, 0, 0) != 0)
            {
               status = -7;
               break;
            }

            ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

            // now setup tunnel to video_render
            if(ilclient_setup_tunnel(tunnel+1, 0, 1000) != 0)
            {
               status = -12;
               break;
            }
            
            ilclient_change_component_state(video_render, OMX_StateExecuting);
         }
         if(!data_len)
            break;

         buf->nFilledLen = data_len;
         data_len = 0;

         buf->nOffset = 0;
         if(first_packet)
         {
            buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
            first_packet = 0;
         }
         else
            buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

         if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
         {
            status = -6;
            break;
         }
         
      }
stop:
      buf->nFilledLen = 0;
      buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

      if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
         status = -20;
      
      // wait for EOS from render
      ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
                              ILCLIENT_BUFFER_FLAG_EOS, 10000);

      // need to flush the renderer to allow video_decode to disable its input port
      ilclient_flush_tunnels(tunnel, 0);
      ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
   }

   ilclient_disable_tunnel(tunnel);
   ilclient_disable_tunnel(tunnel+1);
   ilclient_disable_tunnel(tunnel+2);
   ilclient_teardown_tunnels(tunnel);

   ilclient_state_transition(list, OMX_StateIdle);
   ilclient_state_transition(list, OMX_StateLoaded);

   ilclient_cleanup_components(list);

   OMX_Deinit();

   ilclient_destroy(client);

   DEBUGF("End of omx thread - status=%d\n",status);

   return status;
}

void vcodec_omx_init(struct codec_t* codec, OMX_IMAGE_CODINGTYPE codectype)
{
  codec->codectype = codectype;
  codec_queue_init(codec);
  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)codec);
}
