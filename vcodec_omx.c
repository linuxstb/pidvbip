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
   int is_paused = 0;
   int64_t prev_DTS;
   OMX_BUFFERHEADERTYPE *buf;

   free(args);

   codec->first_packet = 1;

   pthread_mutex_lock(&pipe->omx_active_mutex);

   fprintf(stderr,"Starting vcodec_omx_thread\n");

next_channel:
   fprintf(stderr,"vcodec_omx_thread: next_channel\n");
   coding = OMX_VIDEO_CodingUnused;
   codec->first_packet = 1;
   prev_DTS = -1;

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
       //fprintf(stderr,"[vcodec] getting next item\n");
       current = codec_queue_get_next_item(codec); 
       //fprintf(stderr,"[vcodec] got next item\n");

       if (current->msgtype == MSG_NEW_CHANNEL) {
         codec_queue_free_item(codec,current);
         current = NULL;
         if (pipe->omx_active) {
	   //fprintf(stderr,"[vcodec] NEW_CHANNEL received, restarting pipeline\n");
           goto stop;
	 } else {
	   //fprintf(stderr,"[vcodec] NEW_CHANNEL received, pipeline not active\n");
           pthread_mutex_unlock(&pipe->omx_active_mutex);
           //fprintf(stderr,"[vcodec] unlocked omx_active_mutex\n");
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
       codec->acodec->first_packet = 1;

       /* We are ready to go, allow the audio codec back in */
       pipe->omx_active = 1;
       pthread_cond_signal(&pipe->omx_active_cv);
       //fprintf(stderr,"[vcodec] unlocking omx_active_mutex\n");
       pthread_mutex_unlock(&pipe->omx_active_mutex);
       //fprintf(stderr,"[vcodec] unlocked omx_active_mutex\n");
     } else if ((coding != codec->vcodectype) || (width != codec->width) || (height != codec->height)) {
       fprintf(stderr,"Change of codec detected, restarting video codec\n");
       goto stop;
     }

     int bytes_left = current->data->packetlength;
     unsigned char* p = current->data->packet;
     //fprintf(stderr,"Processing video packet - %d bytes\n",bytes_left);
     while (bytes_left > 0) {
       // fprintf(stderr,"OMX buffers: v: %02d/20 a: %02d/32 free, vcodec queue: %4d, acodec queue: %4d\r",omx_get_free_buffer_count(&pipe->video_decode),omx_get_free_buffer_count(&pipe->audio_render),codec->queue_count, codec->acodec->queue_count);
       DEBUGF( "OMX buffers: v: %02d/20 a: %02d/32 free, vcodec queue: %4d, acodec queue: %4d\r",omx_get_free_buffer_count(&pipe->video_decode),omx_get_free_buffer_count(&pipe->audio_render),codec->queue_count, codec->acodec->queue_count);
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
         fprintf(stderr,"First video packet\n");
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

   //fprintf(stderr,"[vcodec] - waiting for omx_active_mutex\n");
   pthread_mutex_lock(&pipe->omx_active_mutex);
   //fprintf(stderr,"[vcodec] - got omx_active_mutex, tearing down pipeline.\n");

   omx_teardown_pipeline(pipe);
   //fprintf(stderr,"[vcodec] - End of omx thread, pipeline torn down.\n");
   pipe->omx_active = 0;

   goto next_channel;

   return 0;
}

void vcodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pipe)
{
  codec->vcodectype = OMX_VIDEO_CodingUnused;
  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)args);
}
