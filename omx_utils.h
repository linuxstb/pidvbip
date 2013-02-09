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

#ifndef __OMX_UTILS_H
#define __OMX_UTILS_H

#include <IL/OMX_Broadcom.h>
#include <interface/vcos/vcos.h>

#define OMX_MIN(a,b) (((a) < (b)) ? (a) : (b))

/* Macro borrowed from omxtx */
#define OERR(cmd)       do {                                            \
                                OMX_ERRORTYPE oerr = cmd;               \
                                if (oerr != OMX_ErrorNone) {            \
                                        fprintf(stderr, #cmd " failed on line %d: %x\n",  __LINE__, oerr);        \
                                        exit(1);                        \
                                }                                       \
                        } while (0)


/* Macro borrowed from omxplayer */
#define OMX_INIT_STRUCTURE(a) \
    memset(&(a), 0, sizeof(a)); \
    (a).nSize = sizeof(a); \
    (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
    (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
    (a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
    (a).nVersion.s.nStep = OMX_VERSION_STEP


struct omx_cmd_t
{
  OMX_HANDLETYPE *hComponent;
  OMX_COMMANDTYPE Cmd;
  OMX_U32 nParam;
  OMX_PTR pCmdData;
};

struct omx_pipeline_t
{
  OMX_HANDLETYPE video_decode;
  OMX_HANDLETYPE video_scheduler;
  OMX_HANDLETYPE video_render;
  OMX_HANDLETYPE clock;
  OMX_BUFFERHEADERTYPE *video_buffers;

  int port_settings_changed;
  /* Variables for handling asynchronous commands */
  pthread_mutex_t cmd_queue_mutex;
  pthread_cond_t cmd_queue_count_cv;
  pthread_mutex_t vidbuf_mutex;
  int vidbuf_notempty;
  pthread_cond_t vidbuf_notempty_cv;
  pthread_mutex_t video_render_eos_mutex;
  int video_render_eos;
  pthread_cond_t video_render_eos_cv;
  struct omx_cmd_t cmd;
};

OMX_ERRORTYPE omx_setup_pipeline(struct omx_pipeline_t* pipe, OMX_VIDEO_CODINGTYPE video_codec);
OMX_ERRORTYPE omx_send_command_and_wait(struct omx_pipeline_t* pipe, OMX_HANDLETYPE *hComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
OMX_BUFFERHEADERTYPE *get_next_buffer(struct omx_pipeline_t* pipe);
OMX_ERRORTYPE omx_flush_tunnel(struct omx_pipeline_t* pipe, OMX_HANDLETYPE source, int source_port, OMX_HANDLETYPE sink, int sink_port);
void omx_free_buffers(struct omx_pipeline_t *pipe, OMX_HANDLETYPE component, int port);
OMX_ERRORTYPE omx_send_command_and_wait0(struct omx_pipeline_t* pipe, OMX_HANDLETYPE *hComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
OMX_ERRORTYPE omx_send_command_and_wait1(struct omx_pipeline_t* pipe, OMX_HANDLETYPE *hComponent, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
void summarise_buffers(OMX_BUFFERHEADERTYPE *buffers);
int omx_get_free_buffer_count(struct omx_pipeline_t* pipe, OMX_BUFFERHEADERTYPE *buffers);

#endif
