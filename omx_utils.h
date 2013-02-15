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

struct omx_component_t
{
  OMX_HANDLETYPE h;
  OMX_CALLBACKTYPE callbacks;

  /* Variables for handling asynchronous commands */
  struct omx_cmd_t cmd;
  pthread_mutex_t cmd_queue_mutex;
  pthread_cond_t cmd_queue_count_cv;

  /* Pointer to parent pipeline */
  struct omx_pipeline_t* pipe;

  OMX_BUFFERHEADERTYPE *buffers;
  int port_settings_changed;

  pthread_mutex_t buf_mutex;
  int buf_notempty;
  pthread_cond_t buf_notempty_cv;

  pthread_mutex_t eos_mutex;
  int eos;
  pthread_cond_t eos_cv;
};

struct omx_pipeline_t
{
  struct omx_component_t video_decode;
  struct omx_component_t video_scheduler;
  struct omx_component_t video_render;
  struct omx_component_t audio_render;
  struct omx_component_t clock;
  struct omx_component_t image_fx; /* For deinterlacing */

  int do_deinterlace;
  pthread_mutex_t omx_active_mutex;
  int omx_active;
  pthread_cond_t omx_active_cv;
};

OMX_ERRORTYPE omx_init_component(struct omx_pipeline_t* pipe, struct omx_component_t* component, char* compname);
OMX_ERRORTYPE omx_setup_pipeline(struct omx_pipeline_t* pipe, OMX_VIDEO_CODINGTYPE video_codec);
OMX_BUFFERHEADERTYPE *get_next_buffer(struct omx_component_t* component);
OMX_ERRORTYPE omx_flush_tunnel(struct omx_component_t* source, int source_port, struct omx_component_t* sink, int sink_port);
void omx_free_buffers(struct omx_component_t *component, int port);
OMX_ERRORTYPE omx_send_command_and_wait(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
OMX_ERRORTYPE omx_send_command_and_wait0(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
OMX_ERRORTYPE omx_send_command_and_wait1(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData);
void summarise_buffers(OMX_BUFFERHEADERTYPE *buffers);
int omx_get_free_buffer_count(struct omx_component_t* component);
void omx_alloc_buffers(struct omx_component_t *component, int port);
void omx_config_pcm(struct omx_component_t* audio_render, int samplerate, int channels, int bitdepth);
OMX_TICKS pts_to_omx(uint64_t pts);

#endif
