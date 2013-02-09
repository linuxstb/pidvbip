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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "omx_utils.h"
#include "vcodec_omx.h"
#include "codec.h"
#include "debug.h"

/* Convert the presentation timestamp into an OMX compatible format */
static OMX_TICKS pts_to_omx(uint64_t pts)
{
  OMX_TICKS ticks;
  ticks.nLowPart = pts;
  ticks.nHighPart = pts >> 32;
  return ticks;
}

static void* vcodec_omx_thread(struct codec_t* codec)
{
   struct omx_pipeline_t pipe;
   int current_used = 0;
   struct codec_queue_t* current;
   int status = 0;
   unsigned char *data = NULL;
   unsigned int data_len = 0;
   int frames_sent = 0;
   int is_paused = 0;
   int64_t prev_DTS = -1;
   int err;
   OMX_BUFFERHEADERTYPE *buf;
   int first_packet = 1;

   OERR(OMX_Init());

   omx_setup_pipeline(&pipe, codec->codectype);

   while (1)
   {
next_packet:
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

       if ((prev_DTS != -1) && ((prev_DTS + 40000) != current->data->DTS) && ((prev_DTS + 20000) != current->data->DTS)) {
         fprintf(stderr,"DTS discontinuity - DTS=%lld, prev_DTS=%lld (diff = %lld)\n",current->data->DTS,prev_DTS,current->data->DTS-prev_DTS);
       }
       prev_DTS = current->data->DTS;
     }

     if (current->data == NULL) {
       fprintf(stderr,"ERROR: data is NULL (expect segfault!)");
     }

     int bytes_left = current->data->packetlength;
     unsigned char* p = current->data->packet;
     //fprintf(stderr,"Processing video packet - %d bytes\n",bytes_left);
     while (bytes_left > 0) {
       fprintf(stderr,"OMX buffers: %02d/20 free, vcodec queue: %4d, acodec queue: %4d\r",omx_get_free_buffer_count(&pipe,pipe.video_buffers),codec->queue_count, codec->acodec->queue_count);
       buf = get_next_buffer(&pipe);   /* This will block if there are no empty buffers */

       int to_copy = OMX_MIN(bytes_left,buf->nAllocLen);
       //fprintf(stderr,"Copying %d bytes\n",to_copy);

       memcpy(buf->pBuffer, p, to_copy);
       p += to_copy;
       bytes_left -= to_copy;
       buf->nFilledLen = to_copy;

       if(first_packet)
       {
         buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
         first_packet = 0;
       }
       else
         buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

       if (pipe.port_settings_changed == 1)
       {
         pipe.port_settings_changed = 0;

         OERR(OMX_SetupTunnel(pipe.video_decode, 131, pipe.video_scheduler, 10));
         omx_send_command_and_wait(&pipe, pipe.video_decode, OMX_CommandPortEnable, 131, NULL);
         omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandPortEnable, 10, NULL);
         omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
       }

       if (pipe.port_settings_changed == 2)
       {
         pipe.port_settings_changed = 0;
         OERR(OMX_SetupTunnel(pipe.video_scheduler, 11, pipe.video_render, 90));  
         omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandPortEnable, 11, NULL);
         omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandPortEnable, 90, NULL);
         omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
       }

       OERR(OMX_EmptyThisBuffer(pipe.video_decode, buf));
     }

     codec_queue_free_item(codec,current);
     current = NULL;
   }

stop:
   buf = get_next_buffer(&pipe);

   buf->nFilledLen = 0;
   buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
   
   OERR(OMX_EmptyThisBuffer(pipe.video_decode, buf));

   /* NOTE: Three events are sent after the previous command:

      [EVENT] Got an event of type 4 on video_decode 0x426a10 (d1: 83, d2 1)
      [EVENT] Got an event of type 4 on video_scheduler 0x430d10 (d1: b, d2 1)
      [EVENT] Got an event of type 4 on video_render 0x430b30 (d1: 5a, d2 1)  5a = port (90) 1 = OMX_BUFFERFLAG_EOS
   */

   /* Wait for video_render to shutdown */
   pthread_mutex_lock(&pipe.video_render_eos_mutex);
   while (!pipe.video_render_eos)
     pthread_cond_wait(&pipe.video_render_eos_cv,&pipe.video_render_eos_mutex);
   pthread_mutex_unlock(&pipe.video_render_eos_mutex);

   /* Flush all tunnels */
   omx_flush_tunnel(&pipe, pipe.video_decode, 131, pipe.video_scheduler, 10);
   omx_flush_tunnel(&pipe, pipe.video_scheduler, 11, pipe.video_render, 90);
   omx_flush_tunnel(&pipe, pipe.clock, 80, pipe.video_scheduler, 12);

   /* Disable video_decode input port and buffers */
   omx_send_command_and_wait0(&pipe, pipe.video_decode, OMX_CommandPortDisable, 130, NULL);
   omx_free_buffers(&pipe, pipe.video_decode, 130);
   omx_send_command_and_wait1(&pipe, pipe.video_decode, OMX_CommandPortDisable, 130, NULL);

done:
   omx_send_command_and_wait(&pipe, pipe.video_decode, OMX_CommandPortDisable, 131, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandPortDisable, 10, NULL);

   omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandPortDisable, 11, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandPortDisable, 90, NULL);

   /* NOTE: The clock disable doesn't complete until after the video scheduler port is 
      disabled (but it completes before the video scheduler port disabling completes). */
   OERR(OMX_SendCommand(pipe.clock, OMX_CommandPortDisable, 80, NULL));
   omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandPortDisable, 12, NULL);

   /* Teardown tunnels */
   OERR(OMX_SetupTunnel(pipe.video_decode, 131, NULL, 0));
   OERR(OMX_SetupTunnel(pipe.video_scheduler, 10, NULL, 0));

   OERR(OMX_SetupTunnel(pipe.video_scheduler, 11, NULL, 0));
   OERR(OMX_SetupTunnel(pipe.video_render, 90, NULL, 0));

   OERR(OMX_SetupTunnel(pipe.clock, 80, NULL, 0));
   OERR(OMX_SetupTunnel(pipe.video_scheduler, 12, NULL, 0));

   /* Transition all components to Idle */
   omx_send_command_and_wait(&pipe, pipe.video_decode, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe, pipe.clock, OMX_CommandStateSet, OMX_StateIdle, NULL);

   /* Transition all components to Loaded */
   omx_send_command_and_wait(&pipe, pipe.video_decode, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_scheduler, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe, pipe.video_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe, pipe.clock, OMX_CommandStateSet, OMX_StateLoaded, NULL);

   /* Finally free the component handles */
   OERR(OMX_FreeHandle(pipe.video_decode));
   OERR(OMX_FreeHandle(pipe.video_scheduler));
   OERR(OMX_FreeHandle(pipe.video_render));
   OERR(OMX_FreeHandle(pipe.clock));

   //OMX_Deinit();

   DEBUGF("End of omx thread - status=%d\n",status);

   return status;
}

void vcodec_omx_init(struct codec_t* codec, OMX_IMAGE_CODINGTYPE codectype)
{
  codec->codectype = codectype;
  codec_queue_init(codec);
  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)codec);
}
