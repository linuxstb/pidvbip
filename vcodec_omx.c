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
#include "utils.h"

#define SCREENWIDTH 1280
#define SCREENHEIGHT 800

static void* vcodec_omx_thread(struct codec_init_args_t* args)
{
   struct codec_t* codec = args->codec;
   struct omx_pipeline_t* pipe = args->pipe;
   char* audio_dest = args->audio_dest;
   OMX_VIDEO_CODINGTYPE coding;
   int width, height;
   struct codec_queue_t* current = NULL;
   int is_paused = 0;
   int64_t prev_DTS;
   OMX_BUFFERHEADERTYPE *buf;
   int current_aspect;
   int aspect;
   int gopbytes,totalbytes;
   uint64_t gopfirstdts;
   uint64_t firstdts = -1;
   double avg_bitrate, min_bitrate, max_bitrate;

   free(args);

   codec->first_packet = 1;

   pthread_mutex_lock(&pipe->omx_active_mutex);

   fprintf(stderr,"Starting vcodec_omx_thread\n");

next_channel:
   fprintf(stderr,"vcodec_omx_thread: next_channel\n");
   coding = OMX_VIDEO_CodingUnused;
   codec->first_packet = 1;
   prev_DTS = -1;
   current_aspect = 0;
   pipe->video_render.aspect = 0;
   aspect = 0;
   firstdts = -1;
   totalbytes = 0;
   gopbytes = -1;
   min_bitrate = 0;
   max_bitrate = 0;

   while (1)
   {
next_packet:
     if (current == NULL) {
       if (is_paused) {
         // Wait for resume message
         fprintf(stderr,"vcodec: Waiting for resume\n");
         pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
         pthread_mutex_unlock(&codec->queue_mutex);
         omx_clock_set_speed(&pipe->clock, 1<<16);
         is_paused = 0;
       }
       //fprintf(stderr,"[vcodec] getting next item\n\n");
       current = codec_queue_get_next_item(codec); 
       //fprintf(stderr,"[vcodec] got next item\n\n");

       if ((current->msgtype == MSG_NEW_CHANNEL) || (current->msgtype == MSG_STOP)) {
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
         omx_clock_set_speed(&pipe->clock, 0);
         is_paused = 1;
         goto next_packet;
       } else if (current->msgtype == MSG_SET_ASPECT_4_3) {
         omx_set_display_region(pipe, 240, 0, SCREENHEIGHT * 0.75, SCREENHEIGHT);
         current = NULL;
         goto next_packet;
       } else if (current->msgtype == MSG_SET_ASPECT_16_9) {
         omx_set_display_region(pipe, 0, 0, SCREENWIDTH, SCREENHEIGHT);
         current = NULL;
         goto next_packet;
       } else if (current->msgtype == MSG_ZOOM) {
         if ((int)current->data) {
           fprintf(stderr,"4:3 on!\n");
           omx_set_display_region(pipe, 240, 0, SCREENHEIGHT * 0.75,SCREENHEIGHT);
         } else {
           fprintf(stderr,"4:3 off\n");
           omx_set_display_region(pipe, 0, 0, SCREENWIDTH, SCREENHEIGHT);
         }
         current = NULL;
         goto next_packet;
       } else if (current->msgtype == MSG_CROP) {

         if ((int)current->data) {
           int left = 384;
           int bottom = 224;
           omx_set_source_region(pipe, left, 0, SCREENWIDTH-left, SCREENHEIGHT-bottom);
           fprintf(stderr,"Crop on!\n");
         } else {
           fprintf(stderr,"Crop off\n");
           omx_set_source_region(pipe, 0, 0, SCREENWIDTH, SCREENHEIGHT);
         }
         current = NULL;
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

     if (current->data->frametype == 'I') {
       if (firstdts == -1) { firstdts = current->data->DTS; }
       if (gopbytes != -1) {
         double duration = current->data->DTS-gopfirstdts;
         double total_duration = current->data->DTS-firstdts;
         double bitrate = (1000000.0/duration) * gopbytes * 8.0;
         double total_bitrate = (1000000.0/total_duration) * totalbytes * 8.0;
         if ((min_bitrate == 0) || (bitrate < min_bitrate)) { min_bitrate = bitrate; }
         if ((max_bitrate == 0) || (bitrate > max_bitrate)) { max_bitrate = bitrate; }
         fprintf(stderr,"GOP: %d bytes (%dms) - %.3fMbps  (avg: %.3fMbps, min: %.3fMbps, max: %.3fMbps                    \r",gopbytes,(int)(current->data->DTS-gopfirstdts),bitrate/1000000,total_bitrate/1000000,min_bitrate/1000000,max_bitrate/1000000);
       }
       gopbytes = current->data->packetlength;
       gopfirstdts = current->data->DTS;
       totalbytes += current->data->packetlength;
     } else {
       if (gopbytes >= 0)
         gopbytes += current->data->packetlength;
       totalbytes += current->data->packetlength;
     }
     if ((current->data->frametype == 'I') && (codec->vcodectype == OMX_VIDEO_CodingMPEG2)) {
       unsigned char* p = current->data->packet;
       /* Parse the MPEG stream to extract the aspect ratio.
          TODO: Handle the Active Format Description (AFD) which is frame-accurate.  This is just GOP-accurate .

          "AFD is optionally carried in the user data of video elementary bitstreams, after the sequence
          extension, GOP header, and/or picture coding extension."
        */
       if ((p[0]==0) && (p[1]==0) && (p[2]==1) && (p[3]==0xb3)) { // Sequence header
	 //int width = (p[4] << 4) | (p[5] & 0xf0) >> 4;
         //int height = (p[5] & 0x0f) << 8 | p[6];
         aspect = (p[7] & 0xf0) >> 4;

         //fprintf(stderr,"MPEG-2 sequence header - width=%d, height=%d, aspect=%d\n",width,height,aspect);
       }
     }

     /* Check if aspect ratio in video_render component has changed */
     if ((codec->vcodectype == OMX_VIDEO_CodingMPEG2) && (pipe->video_render.aspect != current_aspect)) {
       if (pipe->video_render.aspect == 2) { // 4:3
         fprintf(stderr,"Switching to 4:3\n");
         omx_set_display_region(pipe, 240, 0, SCREENHEIGHT * 0.75, SCREENHEIGHT);
       } else { // 16:9 - DVB can only be 4:3 or 16:9
         fprintf(stderr,"Switching to 16:9\n");
         omx_set_display_region(pipe, 0, 0, SCREENWIDTH, SCREENHEIGHT);
       }
       current_aspect = pipe->video_render.aspect;
     }

     if (coding == OMX_VIDEO_CodingUnused) {
       fprintf(stderr,"Setting up OMX pipeline... - vcodectype=%d\n",codec->vcodectype);
       omx_setup_pipeline(pipe, codec->vcodectype, audio_dest, ((codec->width*codec->height) > 720*576) ? 1 : 0);

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

       buf->hMarkTargetComponent = pipe->video_render.h;
       buf->pMarkData = (OMX_PTR)aspect;

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
         pipe->video_decode.port_settings_changed = 2;
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
         pipe->image_fx.port_settings_changed = 2;
         fprintf(stderr,"image_fx port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pipe->image_fx.h, 191, pipe->video_scheduler.h, 10));
         omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortEnable, 191, NULL);

         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortEnable, 10, NULL);
         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
       }

       if (pipe->video_scheduler.port_settings_changed == 1)
       {
         pipe->video_scheduler.port_settings_changed = 2;
	 fprintf(stderr,"video_scheduler port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 11, pipe->video_render.h, 90));  
         omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortEnable, 11, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandPortEnable, 90, NULL);
         omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);

         fprintf(stderr,"TOTAL CHANNEL CHANGE TIME: %.3fs\n",(get_time()-pipe->channel_switch_starttime)/1000.0);
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
   fprintf(stderr,"OMX teardown complete: %.3fs\n",(get_time()-pipe->channel_switch_starttime)/1000.0);

   goto next_channel;

   return 0;
}

void vcodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pipe, char* audio_dest)
{
  codec->vcodectype = OMX_VIDEO_CodingUnused;
  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pipe;
  args->audio_dest = audio_dest;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)args);
}
