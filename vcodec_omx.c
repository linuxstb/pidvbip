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
#include <string.h>

#include "omx_utils.h"
#include "vcodec_omx.h"
#include "codec.h"
#include "debug.h"

static void* vcodec_omx_thread(struct codec_init_args_t* args)
{
   struct codec_t* codec = args->codec;
   struct omx_pipeline_t* pipe = args->pipe;
   OMX_VIDEO_CODINGTYPE coding;
   int width, height;
   struct codec_queue_t* current = NULL;
   unsigned char *data = NULL;
   unsigned int data_len = 0;
   int frames_sent = 0;
   int is_paused = 0;
   int64_t prev_DTS = -1;
   int err;
   OMX_BUFFERHEADERTYPE *buf;

   free(args);

   codec->first_packet = 1;

   pthread_mutex_lock(&pipe->omx_active_mutex);

   fprintf(stderr,"Starting vcodec_omx_thread\n");

next_channel:
   fprintf(stderr,"vcodec_omx_thread: next_channel\n");
   coding = OMX_VIDEO_CodingUnused;
   codec->first_packet = 1;

   while (1)
   {
next_packet:
     if (current == NULL) {
       if (is_paused) {
         // Wait for resume message
         fprintf(stderr,"vcodec: Waiting for resume\n");
         pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
         pthread_mutex_unlock(&codec->queue_mutex);
         is_paused = 0;
       }
       current = codec_queue_get_next_item(codec); 

       if (current->msgtype == MSG_NEW_CHANNEL) {
         codec_queue_free_item(codec,current);
         current = NULL;
         if (pipe->omx_active) {
           goto stop;
	 } else {
           goto next_channel;
         }
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

     if (coding == OMX_VIDEO_CodingUnused) {
       fprintf(stderr,"Setting up OMX pipeline... - vcodectype=%d\n",codec->vcodectype);
       omx_setup_pipeline(pipe, codec->vcodectype);
 
       fprintf(stderr,"Done setting up OMX pipeline.\n");
       coding = codec->vcodectype;
       width = codec->width;
       height = codec->height;
       fprintf(stderr,"Initialised video codec - %s width=%d, height=%d\n",((coding == OMX_VIDEO_CodingAVC) ? "H264" : "MPEG-2"), width, height);

       /* We are ready to go, allow the audio codec back in */
       pipe->omx_active = 1;
       pthread_cond_signal(&pipe->omx_active_cv);
       pthread_mutex_unlock(&pipe->omx_active_mutex);
     } else if ((coding != codec->vcodectype) || (width != codec->width) || (height != codec->height)) {
       fprintf(stderr,"Change of codec detected, restarting video codec\n");
       goto stop;
     }

     int bytes_left = current->data->packetlength;
     unsigned char* p = current->data->packet;
     //fprintf(stderr,"Processing video packet - %d bytes\n",bytes_left);
     while (bytes_left > 0) {
       fprintf(stderr,"OMX buffers: %02d/20 free, vcodec queue: %4d, acodec queue: %4d\r",omx_get_free_buffer_count(&pipe->video_decode),codec->queue_count, codec->acodec->queue_count);
       buf = get_next_buffer(&pipe->video_decode);   /* This will block if there are no empty buffers */

       int to_copy = OMX_MIN(bytes_left,buf->nAllocLen);
       //fprintf(stderr,"Copying %d bytes\n",to_copy);

       memcpy(buf->pBuffer, p, to_copy);
       p += to_copy;
       bytes_left -= to_copy;
       buf->nTimeStamp = pts_to_omx(current->data->PTS);
       buf->nFilledLen = to_copy;

       buf->nFlags = 0;
       if(codec->first_packet)
       {
         buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
         codec->first_packet = 0;
       }

       if (bytes_left == 0)
	 buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

       if (pipe->video_decode.port_settings_changed == 1)
       {
         pipe->video_decode.port_settings_changed = 0;
	 fprintf(stderr,"video_decode port_settings_changed = 1\n");

         if (pipe->do_deinterlace) {
           OERR(OMX_SetupTunnel(pipe->video_decode.h, 131, pipe->image_fx.h, 190));
           omx_send_command_and_wait(&pipe->video_decode, OMX_CommandPortEnable, 131, NULL);

           omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortEnable, 190, NULL);
           omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         } else {
           OERR(OMX_SetupTunnel(pipe->video_decode.h, 131, pipe->video_scheduler.h, 10));
           omx_send_command_and_wait(&pipe->video_decode, OMX_CommandPortEnable, 131, NULL);

           omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortEnable, 10, NULL);
           omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
           omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
         }
       }

       if (pipe->image_fx.port_settings_changed == 1)
       {
         pipe->image_fx.port_settings_changed = 0;
         fprintf(stderr,"image_fx port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pipe->image_fx.h, 191, pipe->video_scheduler.h, 10));
         omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortEnable, 191, NULL);

         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortEnable, 10, NULL);
         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
       }

       if (pipe->video_scheduler.port_settings_changed == 1)
       {
         pipe->video_scheduler.port_settings_changed = 0;
	 fprintf(stderr,"video_scheduler port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 11, pipe->video_render.h, 90));  
         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortEnable, 11, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandPortEnable, 90, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
       }

       OERR(OMX_EmptyThisBuffer(pipe->video_decode.h, buf));
     }

     codec_queue_free_item(codec,current);
     current = NULL;
   }

stop:
   /* We lock the mutex to stop the audio codec.  It is unlocked after the pipline is setup again */
   pthread_mutex_lock(&pipe->omx_active_mutex);

   /* Indicate end of video stream */
   buf = get_next_buffer(&pipe->video_decode);

   buf->nFilledLen = 0;
   buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
   
   OERR(OMX_EmptyThisBuffer(pipe->video_decode.h, buf));

   /* NOTE: Three events are sent after the previous command:

      [EVENT] Got an event of type 4 on video_decode 0x426a10 (d1: 83, d2 1)
      [EVENT] Got an event of type 4 on video_scheduler 0x430d10 (d1: b, d2 1)
      [EVENT] Got an event of type 4 on video_render 0x430b30 (d1: 5a, d2 1)  5a = port (90) 1 = OMX_BUFFERFLAG_EOS
   */

   /* Wait for video_render to shutdown */
   pthread_mutex_lock(&pipe->video_render.eos_mutex);
   while (!pipe->video_render.eos)
     pthread_cond_wait(&pipe->video_render.eos_cv,&pipe->video_render.eos_mutex);
   pthread_mutex_unlock(&pipe->video_render.eos_mutex);

   /* Flush all tunnels */
   if (pipe->do_deinterlace) {
     omx_flush_tunnel(&pipe->video_decode, 131, &pipe->image_fx, 190);
     omx_flush_tunnel(&pipe->image_fx, 191, &pipe->video_scheduler, 10);
   } else {
     omx_flush_tunnel(&pipe->video_decode, 131, &pipe->video_scheduler, 10);
   }
   omx_flush_tunnel(&pipe->video_scheduler, 11, &pipe->video_render, 90);
   omx_flush_tunnel(&pipe->clock, 80, &pipe->video_scheduler, 12);

   /* Disable video_decode input port and buffers */
   omx_send_command_and_wait0(&pipe->video_decode, OMX_CommandPortDisable, 130, NULL);
   omx_free_buffers(&pipe->video_decode, 130);
   omx_send_command_and_wait1(&pipe->video_decode, OMX_CommandPortDisable, 130, NULL);

   /* Disable audio_render input port and buffers */
   omx_send_command_and_wait0(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);
   omx_free_buffers(&pipe->audio_render, 100);
   omx_send_command_and_wait1(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);

   omx_send_command_and_wait(&pipe->video_decode, OMX_CommandPortDisable, 131, NULL);

   if (pipe->do_deinterlace) {
     omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortDisable, 190, NULL);
     omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortDisable, 191, NULL);
   }

   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 10, NULL);

   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 11, NULL);
   omx_send_command_and_wait(&pipe->video_render, OMX_CommandPortDisable, 90, NULL);

   /* NOTE: The clock disable doesn't complete until after the video scheduler port is 
      disabled (but it completes before the video scheduler port disabling completes). */
   OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortDisable, 80, NULL));
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandPortDisable, 101, NULL);
   OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortDisable, 81, NULL));
   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 12, NULL);

   /* Teardown tunnels */
   OERR(OMX_SetupTunnel(pipe->video_decode.h, 131, NULL, 0));
   if (pipe->do_deinterlace) {
     OERR(OMX_SetupTunnel(pipe->image_fx.h, 190, NULL, 0));
     OERR(OMX_SetupTunnel(pipe->image_fx.h, 191, NULL, 0));
   }
   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 10, NULL, 0));

   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 11, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->video_render.h, 90, NULL, 0));

   OERR(OMX_SetupTunnel(pipe->clock.h, 81, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 12, NULL, 0));

   OERR(OMX_SetupTunnel(pipe->clock.h, 80, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->audio_render.h, 101, NULL, 0));

   /* Transition all components to Idle */
   omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
   omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateIdle, NULL);
   if (pipe->do_deinterlace) { omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateIdle, NULL); }


   /* Transition all components to Loaded */
   omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   if (pipe->do_deinterlace) { omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateLoaded, NULL); }


   /* Finally free the component handles */
   OERR(OMX_FreeHandle(pipe->video_decode.h));
   OERR(OMX_FreeHandle(pipe->video_scheduler.h));
   OERR(OMX_FreeHandle(pipe->video_render.h));
   OERR(OMX_FreeHandle(pipe->audio_render.h));
   OERR(OMX_FreeHandle(pipe->clock.h));
   if (pipe->do_deinterlace) { OERR(OMX_FreeHandle(pipe->image_fx.h)); }

   fprintf(stderr,"End of omx thread\n");
   pipe->omx_active = 0;
   //pthread_mutex_unlock(&pipe->omx_active_mutex);
   //fprintf(stderr,"[vcodec_omx] Unlocked mutex\n");

   goto next_channel;

   return 0;
}

void vcodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->vcodectype = OMX_VIDEO_CodingUnused;
  codec_queue_init(codec);

  pthread_mutex_init(&pipe->omx_active_mutex, NULL);
  pthread_cond_init(&pipe->omx_active_cv, NULL);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)args);
}
