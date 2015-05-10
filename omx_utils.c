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
#include <string.h>

#include "omx_utils.h"
#include "configfile.h"
#include "debug.h"

extern struct configfile_parameters global_settings;

OMX_TICKS pts_to_omx(uint64_t pts)
{
  OMX_TICKS ticks;
  ticks.nLowPart = pts;
  ticks.nHighPart = pts >> 32;
  return ticks;
};

static int is_port_enabled(OMX_HANDLETYPE handle, int port)
{
  OMX_PARAM_PORTDEFINITIONTYPE  portdef;

  OMX_INIT_STRUCTURE(portdef);
  portdef.nPortIndex = port;
  OERR(OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &portdef));

  return (portdef.bEnabled == 0 ? 0 : 1);;
}

static void omx_show_state(struct omx_component_t* component, int port1, int port2, int port3)
{
  OMX_STATETYPE state;
  OMX_PARAM_PORTDEFINITIONTYPE  portdef;

  OERR(OMX_GetState(component->h, &state));

  DEBUGF("%s is in state ",component->name);

  switch (state) {
    case OMX_StateInvalid: DEBUGF("OMX_StateInvalid\n"); break;
    case OMX_StateLoaded: DEBUGF("OMX_StateLoaded\n"); break;
    case OMX_StateIdle: DEBUGF("OMX_StateIdle\n"); break;
    case OMX_StateExecuting: DEBUGF("OMX_StateExecuting\n"); break;
    case OMX_StatePause: DEBUGF("OMX_StatePause\n"); break;
    case OMX_StateWaitForResources: DEBUGF("OMX_StateWaitForResources\n"); break;

    default:
      DEBUGF("0x%08x\n",(unsigned int)state);
  }

  OMX_INIT_STRUCTURE(portdef);

  if (port1) {
    portdef.nPortIndex = port1;
    OERR(OMX_GetParameter(component->h, OMX_IndexParamPortDefinition, &portdef));
    DEBUGF("Port %d is %s\n",port1, (portdef.bEnabled == 0 ? "disabled" : "enabled"));
  }

  if (port2) {
    portdef.nPortIndex = port2;
    OERR(OMX_GetParameter(component->h, OMX_IndexParamPortDefinition, &portdef));
    DEBUGF("Port %d is %s\n",port2, (portdef.bEnabled == 0 ? "disabled" : "enabled"));
  }

  if (port3) {
    portdef.nPortIndex = port3;
    OERR(OMX_GetParameter(component->h, OMX_IndexParamPortDefinition, &portdef));
    DEBUGF("Port %d is %s\n",port3, (portdef.bEnabled == 0 ? "disabled" : "enabled"));
  }
}


/* From omxtx */
/* Print some useful information about the state of the port: */
static void dumpport(OMX_HANDLETYPE handle, int port)
{
  OMX_VIDEO_PORTDEFINITIONTYPE  *viddef;
  OMX_PARAM_PORTDEFINITIONTYPE  portdef;

  OMX_INIT_STRUCTURE(portdef);
  portdef.nPortIndex = port;
  OERR(OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &portdef));

  printf("Port %d is %s, %s\n", portdef.nPortIndex,
    (portdef.eDir == 0 ? "input" : "output"),
    (portdef.bEnabled == 0 ? "disabled" : "enabled"));
  printf("Wants %d bufs, needs %d, size %d, enabled: %d, pop: %d, "
    "aligned %d\n", portdef.nBufferCountActual,
    portdef.nBufferCountMin, portdef.nBufferSize,
    portdef.bEnabled, portdef.bPopulated,
    portdef.nBufferAlignment);
  viddef = &portdef.format.video;

  switch (portdef.eDomain) {
  case OMX_PortDomainVideo:
    printf("Video type is currently:\n"
      "\tMIME:\t\t%s\n"
      "\tNative:\t\t%p\n"
      "\tWidth:\t\t%d\n"
      "\tHeight:\t\t%d\n"
      "\tStride:\t\t%d\n"
      "\tSliceHeight:\t%d\n"
      "\tBitrate:\t%d\n"
      "\tFramerate:\t%d (%x); (%f)\n"
      "\tError hiding:\t%d\n"
      "\tCodec:\t\t%d\n"
      "\tColour:\t\t%d\n",
      viddef->cMIMEType, viddef->pNativeRender,
      viddef->nFrameWidth, viddef->nFrameHeight,
      viddef->nStride, viddef->nSliceHeight,
      viddef->nBitrate,
      viddef->xFramerate, viddef->xFramerate,
      ((float)viddef->xFramerate/(float)65536),
      viddef->bFlagErrorConcealment,
      viddef->eCompressionFormat, viddef->eColorFormat);
    break;
  case OMX_PortDomainImage:
    printf("Image type is currently:\n"
      "\tMIME:\t\t%s\n"
      "\tNative:\t\t%p\n"
      "\tWidth:\t\t%d\n"
      "\tHeight:\t\t%d\n"
      "\tStride:\t\t%d\n"
      "\tSliceHeight:\t%d\n"
      "\tError hiding:\t%d\n"
      "\tCodec:\t\t%d\n"
      "\tColour:\t\t%d\n",
      portdef.format.image.cMIMEType,
      portdef.format.image.pNativeRender,
      portdef.format.image.nFrameWidth,
      portdef.format.image.nFrameHeight,
      portdef.format.image.nStride,
      portdef.format.image.nSliceHeight,
      portdef.format.image.bFlagErrorConcealment,
      portdef.format.image.eCompressionFormat,
      portdef.format.image.eColorFormat);     
    break;
/* Feel free to add others. */
  default:
    break;
  }
}

OMX_ERRORTYPE omx_send_command_and_wait0(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData)
{
   pthread_mutex_lock(&component->cmd_queue_mutex);
   component->cmd.hComponent = component->h;
   component->cmd.Cmd = Cmd;
   component->cmd.nParam = nParam;
   component->cmd.pCmdData = pCmdData;
   pthread_mutex_unlock(&component->cmd_queue_mutex);

   OMX_SendCommand(component->h, Cmd, nParam, pCmdData);
}

OMX_ERRORTYPE omx_send_command_and_wait1(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData)
{
   pthread_mutex_lock(&component->cmd_queue_mutex);
   while (component->cmd.hComponent) {
     /* pthread_cond_wait releases the mutex (which must be locked) and blocks on the condition variable */
     pthread_cond_wait(&component->cmd_queue_count_cv,&component->cmd_queue_mutex);
   }
   pthread_mutex_unlock(&component->cmd_queue_mutex);

   //fprintf(stderr,"Command completed\n");
}

OMX_ERRORTYPE omx_send_command_and_wait(struct omx_component_t* component, OMX_COMMANDTYPE Cmd, OMX_U32 nParam, OMX_PTR pCmdData)
{
  omx_send_command_and_wait0(component,Cmd,nParam,pCmdData);
  omx_send_command_and_wait1(component,Cmd,nParam,pCmdData);
}

//void omx_wait_for_event(struct omx_pipeleine_t* pipe, OMX_HANDLETYPE *hComponent
//			//&pipe,video_render, OMXEventBufferFlag, 90, OMX_BUFFERFLAG_EOS)
//{
//
//}

/* The event handler is called from the OMX component thread */
static OMX_ERRORTYPE omx_event_handler(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_IN OMX_PTR pAppData,
                                       OMX_IN OMX_EVENTTYPE eEvent,
                                       OMX_IN OMX_U32 nData1,
                                       OMX_IN OMX_U32 nData2,
                                       OMX_IN OMX_PTR pEventData)
{
  struct omx_component_t* component = (struct omx_component_t*)pAppData;

//  fprintf(stderr,"[EVENT]: threadid=%u\n",(unsigned int)pthread_self());

  switch (eEvent) {
    case OMX_EventError:
      fprintf(stderr,"[EVENT] %s %p has errored: %x\n", component->name, hComponent, (unsigned int)nData1);
      exit(1);
//fprintf(stderr,"[EVENT] Waiting for lock\n");
      pthread_mutex_lock(&component->cmd_queue_mutex);
//fprintf(stderr,"[EVENT] Got lock - cmd.hComponent=%x\n",(unsigned int)pipe->cmd.hComponent);
      memset(&component->cmd,0,sizeof(component->cmd));
//fprintf(stderr,"[EVENT] Clearing cmd\n");
      pthread_cond_signal(&component->cmd_queue_count_cv);
      pthread_mutex_unlock(&component->cmd_queue_mutex);
//fprintf(stderr,"[EVENT] Returning from event\n");
      return nData1;
      break;

    case OMX_EventCmdComplete:
      DEBUGF("[EVENT] %s %p has completed the last command (%x).\n", component->name, hComponent, nData1);

//fprintf(stderr,"[EVENT] Waiting for lock\n");
      pthread_mutex_lock(&component->cmd_queue_mutex);
//fprintf(stderr,"[EVENT] Got lock\n");
      if ((nData1 == component->cmd.Cmd) &&
          (nData2 == component->cmd.nParam)) {
         memset(&component->cmd,0,sizeof(component->cmd));
         pthread_cond_signal(&component->cmd_queue_count_cv);
      }
      pthread_mutex_unlock(&component->cmd_queue_mutex);

      break;

    case OMX_EventPortSettingsChanged: 
      DEBUGF("[EVENT] %s %p port %d settings changed.\n", component->name, hComponent, (unsigned int)nData1);
      if (component->port_settings_changed == 0) { component->port_settings_changed = 1; }
      break;

  case OMX_EventBufferFlag:
    DEBUGF("[EVENT] Got an EOS event on %s %p (port %d, d2 %x)\n", component->name, hComponent, (unsigned int)nData1, (unsigned int)nData2);

    if (nData2 & OMX_BUFFERFLAG_EOS) {
      pthread_mutex_lock(&component->eos_mutex);
      component->eos = 1;
      pthread_cond_signal(&component->eos_cv);
      pthread_mutex_unlock(&component->eos_mutex);
      DEBUGF("Sent cond signal for EOS event\n");
    }

    break;

  case OMX_EventMark:
    component->aspect = (OMX_MARKTYPE*)pEventData;
    //fprintf(stderr,"[EVENT] OMX_EventMark - component->aspect=%d\n",component->aspect);
    break;

  case OMX_EventParamOrConfigChanged:
    DEBUGF("[EVENT] OMX_EventParamOrConfigChanged on component \"%s\" - d1=%x, d2=%x\n",component->name,(unsigned int)nData1, (unsigned int)nData2);
    component->config_changed = 1;
    break;

    default:
      DEBUGF("[EVENT] Got an event of type %x on %s %p (d1: %x, d2 %x)\n", eEvent,
       component->name, hComponent, (unsigned int)nData1, (unsigned int)nData2);
  }
        
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE omx_empty_buffer_done(OMX_IN OMX_HANDLETYPE hComponent,
                                           OMX_IN OMX_PTR pAppData,
                                           OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  struct omx_component_t* component = (struct omx_component_t*)pAppData;

  if (component->buf_notempty == 0) {
    pthread_mutex_lock(&component->buf_mutex);
    component->buf_notempty = 1;
    pthread_cond_signal(&component->buf_notempty_cv);
    pthread_mutex_unlock(&component->buf_mutex);
  }
  return OMX_ErrorNone;
}

void omx_clock_set_speed(struct omx_component_t *clock, int v)
{
  OMX_TIME_CONFIG_SCALETYPE scale;
  OMX_INIT_STRUCTURE(scale);

  scale.xScale = v;
  OERR(OMX_SetConfig(clock->h, OMX_IndexConfigTimeScale, &scale));
}

static OMX_ERRORTYPE omx_fill_buffer_done(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_PTR pAppData,
                                          OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
  DEBUGF("[omx_fill_buffer_done]\n");
  return OMX_ErrorNone;
}

/* Function based on ilclient_create_component() */
static void omx_disable_all_ports(struct omx_component_t* component)
{
  OMX_PORT_PARAM_TYPE ports;
  OMX_INDEXTYPE types[] = {OMX_IndexParamAudioInit, OMX_IndexParamVideoInit, OMX_IndexParamImageInit, OMX_IndexParamOtherInit};
  int i;

  ports.nSize = sizeof(OMX_PORT_PARAM_TYPE);
  ports.nVersion.nVersion = OMX_VERSION;

  for(i=0; i<4; i++) {
    OMX_ERRORTYPE error = OMX_GetParameter(component->h, types[i], &ports);
    if(error == OMX_ErrorNone) {
      uint32_t j;
      for(j=0; j<ports.nPorts; j++) {
        //fprintf(stderr,"Disabling port %lu\n",ports.nStartPortNumber+j);
        omx_send_command_and_wait(component, OMX_CommandPortDisable, ports.nStartPortNumber+j, NULL);
      }
    }
  }
}

/* Based on allocbufs from omxtx.
   Buffers are connected as a one-way linked list using pAppPrivate as the pointer to the next element */
void omx_alloc_buffers(struct omx_component_t *component, int port)
{
  int i;
  OMX_BUFFERHEADERTYPE *list = NULL, **end = &list;
  OMX_PARAM_PORTDEFINITIONTYPE portdef;

  OMX_INIT_STRUCTURE(portdef);
  portdef.nPortIndex = port;

  OERR(OMX_GetParameter(component->h, OMX_IndexParamPortDefinition, &portdef));

  if (component == &component->pipe->audio_render) {
    DEBUGF("Allocating %d buffers of %d bytes\n",(int)portdef.nBufferCountActual,(int)portdef.nBufferSize);
    DEBUGF("portdef.bEnabled=%d\n",portdef.bEnabled);
  }

  for (i = 0; i < portdef.nBufferCountActual; i++) {
    OMX_U8 *buf;

    buf = vcos_malloc_aligned(portdef.nBufferSize, portdef.nBufferAlignment, "buffer");

    //    printf("Allocated a buffer of %u bytes\n",(unsigned int)portdef.nBufferSize);

    OERR(OMX_UseBuffer(component->h, end, port, NULL, portdef.nBufferSize, buf));

    end = (OMX_BUFFERHEADERTYPE **) &((*end)->pAppPrivate);
  }

  component->buffers = list;
}

void omx_free_buffers(struct omx_component_t *component, int port)
{
  OMX_BUFFERHEADERTYPE *buf, *prev;
  int i=0;

  buf = component->buffers;
  while (buf) {
    prev = buf->pAppPrivate;
    OERR(OMX_FreeBuffer(component->h, port, buf)); /* This also calls free() */
    buf = prev;
  }
}

int omx_get_free_buffer_count(struct omx_component_t* component)
{
  int n = 0;
  OMX_BUFFERHEADERTYPE *buf = component->buffers;

  pthread_mutex_lock(&component->buf_mutex);
  while (buf) {
    if (buf->nFilledLen == 0) n++;
    buf = buf->pAppPrivate;
  }
  pthread_mutex_unlock(&component->buf_mutex);

  return n;
}

static void dump_buffer_status(OMX_BUFFERHEADERTYPE *buffers)
{
  OMX_BUFFERHEADERTYPE *buf = buffers;

  while (buf) {
    fprintf(stderr,"*****\n");
    fprintf(stderr,"buf->pAppPrivate=%u\n",(unsigned int)buf->pAppPrivate);
    fprintf(stderr,"buf->nAllocLen=%u\n",(unsigned int)buf->nAllocLen);
    fprintf(stderr,"buf->nFilledLen=%u\n",(unsigned int)buf->nFilledLen);
    fprintf(stderr,"buf->pBuffer=%u\n",(unsigned int)buf->pBuffer);
    fprintf(stderr,"buf->nOffset=%u\n",(unsigned int)buf->nOffset);
    fprintf(stderr,"buf->nFlags=%u\n",(unsigned int)buf->nFlags);
    fprintf(stderr,"buf->nInputPortIndex=%u\n",(unsigned int)buf->nInputPortIndex);
    fprintf(stderr,"buf->nOutputPortIndex=%u\n",(unsigned int)buf->nOutputPortIndex);
    buf = buf->pAppPrivate;
  }
}

void summarise_buffers(OMX_BUFFERHEADERTYPE *buffers)
{
  OMX_BUFFERHEADERTYPE *buf = buffers;

  fprintf(stderr,"*******\n");
  while (buf) {
    fprintf(stderr,"buf->nFilledLen=%u\n",(unsigned int)buf->nFilledLen);
    buf = buf->pAppPrivate;
  }
}

/* Return the next free buffer, or NULL if none are free */
OMX_BUFFERHEADERTYPE *get_next_buffer(struct omx_component_t* component)
{
  OMX_BUFFERHEADERTYPE *ret;

retry:
  pthread_mutex_lock(&component->buf_mutex);

  ret = component->buffers;
  while (ret && ret->nFilledLen > 0)
    ret = ret->pAppPrivate;

  if (!ret)
    component->buf_notempty = 0;

  if (ret) {
    pthread_mutex_unlock(&component->buf_mutex);
    return ret;
  }

  //summarise_buffers(pipe->video_buffers);
  while (component->buf_notempty == 0)
     pthread_cond_wait(&component->buf_notempty_cv,&component->buf_mutex);

  pthread_mutex_unlock(&component->buf_mutex);

  goto retry;

  /* We never get here, but keep GCC happy */
  return NULL;
}

OMX_ERRORTYPE omx_flush_tunnel(struct omx_component_t* source, int source_port, struct omx_component_t* sink, int sink_port)
{
  omx_send_command_and_wait(source,OMX_CommandFlush,source_port,NULL);
  omx_send_command_and_wait(sink,OMX_CommandFlush,sink_port,NULL);
}

static void omx_config_pcm(struct omx_component_t* audio_render, int samplerate, int channels, int bitdepth, char* dest)
{
  OMX_AUDIO_PARAM_PCMMODETYPE pcm;
  int32_t s;

  OMX_INIT_STRUCTURE(pcm);
  pcm.nPortIndex = 100;
  pcm.nChannels = channels;
  pcm.eNumData = OMX_NumericalDataSigned;
  pcm.eEndian = OMX_EndianLittle;
  pcm.nSamplingRate = samplerate;
  pcm.bInterleaved = OMX_TRUE;
  pcm.nBitPerSample = bitdepth;
  pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;

  switch(channels) {
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

  OERR(OMX_SetParameter(audio_render->h, OMX_IndexParamAudioPcm, &pcm));

  OMX_CONFIG_BRCMAUDIODESTINATIONTYPE ar_dest;
  OMX_INIT_STRUCTURE(ar_dest);
  strcpy((char *)ar_dest.sName, dest);
  OERR(OMX_SetConfig(audio_render->h, OMX_IndexConfigBrcmAudioDestination, &ar_dest));
}

void omx_set_source_region(struct omx_pipeline_t* pipe, int x, int y, int width, int height)
{
  OMX_CONFIG_DISPLAYREGIONTYPE region;

  OMX_INIT_STRUCTURE(region);
  region.nPortIndex = 90; /* Video render input port */

  region.set = OMX_DISPLAY_SET_SRC_RECT;

  region.src_rect.x_offset = x;
  region.src_rect.y_offset = y;
  region.src_rect.width = width;
  region.src_rect.height = height;

  DEBUGF("Setting source region\n");
  OERR(OMX_SetParameter(pipe->video_render.h, OMX_IndexConfigDisplayRegion, &region));
}

void omx_set_display_region(struct omx_pipeline_t* pipe, int x, int y, int width, int height)
{
  OMX_CONFIG_DISPLAYREGIONTYPE region;

  OMX_INIT_STRUCTURE(region);
  region.nPortIndex = 90; /* Video render input port */

  region.set = OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_SRC_RECT | OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_NOASPECT;

  region.fullscreen = OMX_FALSE;
  region.noaspect = OMX_TRUE;

  region.dest_rect.x_offset = x;
  region.dest_rect.y_offset = y;
  region.dest_rect.width = width;
  region.dest_rect.height = height;

  int crop_left = 192;
  int crop_bottom = 96;
  region.src_rect.x_offset = 1920-crop_left;
  region.src_rect.y_offset = 0;
  region.src_rect.width = width-crop_left;
  region.src_rect.height = height-crop_bottom;

  DEBUGF("Setting display region\n");
  OERR(OMX_SetParameter(pipe->video_render.h, OMX_IndexConfigDisplayRegion, &region));
}

OMX_ERRORTYPE omx_init_component(struct omx_pipeline_t* pipe, struct omx_component_t* component, char* compname)
{
  memset(component,0,sizeof(struct omx_component_t));

  pthread_mutex_init(&component->cmd_queue_mutex, NULL);
  pthread_cond_init(&component->cmd_queue_count_cv,NULL);
  component->buf_notempty = 1;
  pthread_cond_init(&component->buf_notempty_cv,NULL);
  pthread_cond_init(&component->eos_cv,NULL);
  pthread_mutex_init(&component->eos_mutex,NULL);

  component->callbacks.EventHandler = omx_event_handler;
  component->callbacks.EmptyBufferDone = omx_empty_buffer_done;
  component->callbacks.FillBufferDone = omx_fill_buffer_done;

  component->pipe = pipe;

  component->name = compname;

  /* Create OMX component */
  OERR(OMX_GetHandle(&component->h, compname, component, &component->callbacks));

  /* Disable all ports */
  omx_disable_all_ports(component);

}


/* From: http://home.nouwen.name/RaspberryPi/documentation/ilcomponents/camera.html

 In order to optimise loading of relevant drivers, the recommended initialisation sequence is:

    Create component.
    Use OMX_IndexConfigRequestCallback to request callbacks on OMX_IndexParamCameraDeviceNumber.
    Set OMX_IndexParamISPTunerName.
    Set OMX_IndexParamCameraFlashType.
    Set OMX_IndexParamCameraDeviceNumber.
    Wait for the callback that OMX_IndexParamCameraDeviceNumber has changed. At this point, all the drivers have been loaded. Other settings can be applied whilst waiting for this event.
    Query for OMX_IndexConfigCameraSensorModes as required.
    Change state to IDLE, and proceed as required. 
*/

OMX_ERRORTYPE omx_setup_camera_pipeline(struct omx_pipeline_t* pipe)
{

  // Create component.
  omx_init_component(pipe, &pipe->camera, "OMX.broadcom.camera");

  // Use OMX_IndexConfigRequestCallback to request callbacks on OMX_IndexParamCameraDeviceNumber.
  OMX_CONFIG_REQUESTCALLBACKTYPE cbtype;
  OMX_INIT_STRUCTURE(cbtype);
  cbtype.nPortIndex=OMX_ALL;
  cbtype.nIndex=OMX_IndexParamCameraDeviceNumber;
  cbtype.bEnable = OMX_TRUE;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigRequestCallback, &cbtype));

  // Set OMX_IndexParamISPTunerName.

  // Set OMX_IndexParamCameraFlashType.

  // Set OMX_IndexParamCameraDeviceNumber.
  OMX_PARAM_U32TYPE device;
  OMX_INIT_STRUCTURE(device);
  device.nPortIndex = OMX_ALL;
  device.nU32 = 0;
  OERR(OMX_SetParameter(pipe->camera.h, OMX_IndexParamCameraDeviceNumber, &device));

  dumpport(pipe->camera.h, 71);

  /* Set the resolution */
  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  OMX_INIT_STRUCTURE(portdef);
  portdef.nPortIndex = 71;
  OERR(OMX_GetParameter(pipe->camera.h, OMX_IndexParamPortDefinition, &portdef));
  portdef.format.image.nFrameWidth = 640;
  portdef.format.image.nFrameHeight = 360;
  portdef.format.image.nStride = 640;
  OERR(OMX_SetParameter(pipe->camera.h, OMX_IndexParamPortDefinition, &portdef));

  /* Set the framerate */
  OMX_CONFIG_FRAMERATETYPE framerate;
  OMX_INIT_STRUCTURE(framerate);
  framerate.nPortIndex = 71;
  framerate.xEncodeFramerate = 25 << 16; // Q16 format - 25fps
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigVideoFramerate, &framerate));

  /* Set the sharpness */
  OMX_CONFIG_SHARPNESSTYPE sharpness;
  OMX_INIT_STRUCTURE(sharpness);
  sharpness.nPortIndex = OMX_ALL;
  sharpness.nSharpness = -50; /* -100 to 100 */
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonSharpness, &sharpness));

  /* Set the contrast */
  OMX_CONFIG_CONTRASTTYPE contrast;
  OMX_INIT_STRUCTURE(contrast);
  contrast.nPortIndex = OMX_ALL;
  contrast.nContrast = -10; /* -100 to 100 */
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonContrast, &contrast));

  /* Set the brightness */
  OMX_CONFIG_BRIGHTNESSTYPE brightness;
  OMX_INIT_STRUCTURE(brightness);
  brightness.nPortIndex = OMX_ALL;
  brightness.nBrightness = 50; /* 0 to 100 */
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonBrightness, &brightness));

  /* Set the saturation */
  OMX_CONFIG_SATURATIONTYPE saturation;
  OMX_INIT_STRUCTURE(saturation);
  saturation.nPortIndex = OMX_ALL;
  saturation.nSaturation = 0; /* -100 to 100 */
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonSaturation, &saturation));

  /* Video stabilisation */
  OMX_CONFIG_FRAMESTABTYPE framestab;
  OMX_INIT_STRUCTURE(framestab);
  framestab.nPortIndex = OMX_ALL;
  framestab.bStab = OMX_FALSE;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonFrameStabilisation, &framestab));

  /* Set EV compensation, ISO and metering mode */
  OMX_CONFIG_EXPOSUREVALUETYPE exposurevalue;
  OMX_INIT_STRUCTURE(exposurevalue);
  exposurevalue.nPortIndex = OMX_ALL;
  OERR(OMX_GetConfig(pipe->camera.h, OMX_IndexConfigCommonExposureValue, &exposurevalue));
  fprintf(stderr,"nSensitivity=%d\n",exposurevalue.nSensitivity);
  exposurevalue.xEVCompensation = 0;  /* Fixed point value stored as Q16 */
  exposurevalue.nSensitivity = 100;         /**< e.g. nSensitivity = 100 implies "ISO 100" */
  exposurevalue.bAutoSensitivity = OMX_FALSE;
  exposurevalue.eMetering = OMX_MeteringModeAverage; 
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonExposureValue, &exposurevalue));

  /* Set exposure mode */
  OMX_CONFIG_EXPOSURECONTROLTYPE exposure;
  OMX_INIT_STRUCTURE(exposure);
  exposure.nPortIndex = OMX_ALL;
  exposure.eExposureControl = OMX_ExposureControlAuto;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonExposure, &exposure));

  /* Set AWB mode */
  OMX_CONFIG_WHITEBALCONTROLTYPE awb;
  OMX_INIT_STRUCTURE(awb);
  awb.nPortIndex = OMX_ALL;
  awb.eWhiteBalControl = OMX_WhiteBalControlAuto;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonWhiteBalance, &awb));
  
  /* Set image effect */
  OMX_CONFIG_IMAGEFILTERTYPE imagefilter;
  OMX_INIT_STRUCTURE(imagefilter);
  imagefilter.nPortIndex = OMX_ALL;
  imagefilter.eImageFilter = OMX_ImageFilterNone;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonImageFilter, &imagefilter));

  /* Set colour effect */
  OMX_CONFIG_COLORENHANCEMENTTYPE colour;
  OMX_INIT_STRUCTURE(colour);
  colour.nPortIndex = OMX_ALL;
  colour.bColorEnhancement = OMX_FALSE;
  colour.nCustomizedU = 128;
  colour.nCustomizedV = 128;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigCommonColorEnhancement, &colour));

  /* Turn off the LED - doesn't work! */
  OMX_CONFIG_PRIVACYINDICATORTYPE privacy;
  OMX_INIT_STRUCTURE(privacy);
  privacy.ePrivacyIndicatorMode = OMX_PrivacyIndicatorOff;
  OERR(OMX_SetConfig(pipe->camera.h, OMX_IndexConfigPrivacyIndicator, &privacy));

  // Wait for the callback that OMX_IndexParamCameraDeviceNumber has
  // changed. At this point, all the drivers have been loaded. Other
  // settings can be applied whilst waiting for this event.
  fprintf(stderr,"Waiting for camera config to change\n");
  while (!pipe->camera.config_changed);  /* TODO: Use a condition variable */
  fprintf(stderr,"Config changed\n");

  // Query for OMX_IndexConfigCameraSensorModes as required.

  // Change state to IDLE, and proceed as required. 
  omx_send_command_and_wait(&pipe->camera, OMX_CommandStateSet, OMX_StateIdle, NULL);

  OMX_CONFIG_PORTBOOLEANTYPE cameraport;
  OMX_INIT_STRUCTURE(cameraport);
  cameraport.nPortIndex = 71;
  cameraport.bEnabled = OMX_TRUE;
  OERR(OMX_SetParameter(pipe->camera.h, OMX_IndexConfigPortCapturing, &cameraport));

  omx_init_component(pipe, &pipe->video_render, "OMX.broadcom.video_render");
  omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);

  OERR(OMX_SetupTunnel(pipe->camera.h, 71, pipe->video_render.h, 90));  /* Camera capture port to video render */

  omx_send_command_and_wait(&pipe->camera, OMX_CommandPortEnable, 71, NULL);
  omx_send_command_and_wait(&pipe->video_render, OMX_CommandPortEnable, 90, NULL);

  omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
  omx_send_command_and_wait(&pipe->camera, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  omx_set_display_region(pipe, 1200, 180, 640, 360);

  OMX_CONFIG_DISPLAYREGIONTYPE region;
  OMX_INIT_STRUCTURE(region);
  region.nPortIndex = 90; /* Video render input port */
  region.set = OMX_DISPLAY_SET_LAYER;
  region.layer = 10;
  OERR(OMX_SetParameter(pipe->video_render.h, OMX_IndexConfigDisplayRegion, &region));

  fprintf(stderr,"Camera pipeline configured\n");

  dumpport(pipe->camera.h, 71);
}

OMX_ERRORTYPE omx_setup_pipeline(struct omx_pipeline_t* pipe, OMX_VIDEO_CODINGTYPE video_codec, char* audio_dest, int is_hd)
{
  OMX_VIDEO_PARAM_PORTFORMATTYPE format;
  OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;

  OMX_CONFIG_BOOLEANTYPE configBoolTrue;
  OMX_INIT_STRUCTURE(configBoolTrue);
  configBoolTrue.bEnabled = OMX_TRUE;

  pipe->do_deinterlace = 0;

  if (((is_hd == 0) && (global_settings.deinterlace_sd)) || ((is_hd == 1) && (global_settings.deinterlace_hd))) {
    DEBUGF("Enabling de-interlace\n");
    pipe->do_deinterlace = 1;
  }

  omx_init_component(pipe, &pipe->video_decode, "OMX.broadcom.video_decode");
  omx_init_component(pipe, &pipe->video_render, "OMX.broadcom.video_render");

  if (pipe->do_deinterlace) {
    DEBUGF("Enabling de-interlacer\n");
    /* De-interlacer.  Input port 190, Output port 191.  Insert between decoder and scheduler */
    omx_init_component(pipe, &pipe->image_fx, "OMX.broadcom.image_fx");

    /* Configure image_fx */
    omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateIdle, NULL);

    OMX_CONFIG_IMAGEFILTERPARAMSTYPE imagefilter;
    OMX_INIT_STRUCTURE(imagefilter);
    imagefilter.nPortIndex=191;
    imagefilter.nNumParams=1;
    imagefilter.nParams[0]=3; //???
    imagefilter.eImageFilter=OMX_ImageFilterDeInterlaceAdvanced;

    OERR(OMX_SetConfig(pipe->image_fx.h, OMX_IndexConfigCommonImageFilterParameters, &imagefilter));
  } else {
    memset(&pipe->image_fx,0,sizeof(struct omx_component_t));
  }


  omx_init_component(pipe, &pipe->clock, "OMX.broadcom.clock");

  OMX_INIT_STRUCTURE(cstate);
  cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
  cstate.nWaitMask = OMX_CLOCKPORT0|OMX_CLOCKPORT1;
  OERR(OMX_SetParameter(pipe->clock.h, OMX_IndexConfigTimeClockState, &cstate));

  OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE refClock;
  OMX_INIT_STRUCTURE(refClock);
  refClock.eClock = OMX_TIME_RefClockAudio;
  OERR(OMX_SetConfig(pipe->clock.h, OMX_IndexConfigTimeActiveRefClock, &refClock));

  omx_init_component(pipe, &pipe->video_scheduler, "OMX.broadcom.video_scheduler");

  /* Initialise audio output - hardcoded to 48000/Stereo/16-bit */
  omx_init_component(pipe, &pipe->audio_render, "OMX.broadcom.audio_render");

  OMX_PARAM_PORTDEFINITIONTYPE param;
  OMX_INIT_STRUCTURE(param);
  param.nPortIndex = 100;

  OERR(OMX_GetParameter(pipe->audio_render.h, OMX_IndexParamPortDefinition, &param));
  param.nBufferSize = 8192;  /* Needs to be big enough for one frame of data */
  param.nBufferCountActual = 32; /* Arbitrary */
  OERR(OMX_SetParameter(pipe->audio_render.h, OMX_IndexParamPortDefinition, &param));

  omx_config_pcm(&pipe->audio_render, 48000, 2, 16, audio_dest);

  OERR(OMX_SetConfig(pipe->audio_render.h, OMX_IndexConfigBrcmClockReferenceSource, &configBoolTrue));

  omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL);

  omx_send_command_and_wait0(&pipe->audio_render, OMX_CommandPortEnable, 100, NULL);
  omx_alloc_buffers(&pipe->audio_render, 100);
  omx_send_command_and_wait1(&pipe->audio_render, OMX_CommandPortEnable, 100, NULL);


  /* Setup clock tunnels first */
  omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateIdle, NULL);

  OERR(OMX_SetupTunnel(pipe->clock.h, 80, pipe->audio_render.h, 101));
  OERR(OMX_SetupTunnel(pipe->clock.h, 81, pipe->video_scheduler.h, 12));

  OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortEnable, 80, NULL));
  OERR(OMX_SendCommand(pipe->video_scheduler.h, OMX_CommandPortEnable, 12, NULL));

  OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortEnable, 81, NULL));
  OERR(OMX_SendCommand(pipe->audio_render.h, OMX_CommandPortEnable, 101, NULL));

  omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateIdle, NULL);

  omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Configure video_decoder */
  omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateIdle, NULL);

  /* Enable lazy image pool destroying */
  OERR(OMX_SetConfig(pipe->video_decode.h, OMX_IndexParamBrcmLazyImagePoolDestroy, &configBoolTrue));

  OMX_INIT_STRUCTURE(format);
  format.nPortIndex = 130;
  format.eCompressionFormat = video_codec;

  OERR(OMX_SetParameter(pipe->video_decode.h, OMX_IndexParamVideoPortFormat, &format));

   /* Enable error concealment for H264 only - without this, HD channels don't work reliably */
  if (video_codec == OMX_VIDEO_CodingAVC) {
     OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE ec;
     OMX_INIT_STRUCTURE(ec);
     ec.bStartWithValidFrame = OMX_FALSE;
     OERR(OMX_SetParameter(pipe->video_decode.h, OMX_IndexParamBrcmVideoDecodeErrorConcealment, &ec));
  }

  /* Enable video decoder input port */
  omx_send_command_and_wait0(&pipe->video_decode, OMX_CommandPortEnable, 130, NULL);

  /* Allocate input buffers */
  omx_alloc_buffers(&pipe->video_decode, 130);

  /* Wait for input port to be enabled */
  omx_send_command_and_wait1(&pipe->video_decode, OMX_CommandPortEnable, 130, NULL);

  /* Change video_decode to OMX_StateExecuting */
  omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Change audio_render to OMX_StateExecuting */
  omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);

  /* Enable passing of buffer marks */
  OERR(OMX_SetParameter(pipe->video_decode.h, OMX_IndexParamPassBufferMarks, &configBoolTrue));
  OERR(OMX_SetParameter(pipe->video_render.h, OMX_IndexParamPassBufferMarks, &configBoolTrue));

  return OMX_ErrorNone;
}

void omx_teardown_pipeline(struct omx_pipeline_t* pipe)
{
   OMX_BUFFERHEADERTYPE *buf;
   int i=1;

   DEBUGF("[vcodec] omx_teardown pipeline:\n");
   DEBUGF("pipe->video_decode.port_settings_changed = %d\n",pipe->video_decode.port_settings_changed);
   DEBUGF("pipe->image_fx.port_settings_changed = %d\n",pipe->image_fx.port_settings_changed);
   DEBUGF("pipe->video_scheduler.port_settings_changed = %d\n",pipe->video_scheduler.port_settings_changed);
   //dumpport(pipe->video_decode.h,130);

#if 0
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

#endif

#if 0
   DEBUGF("[vcodec] omx_teardown pipeline 2\n");
   /* Wait for video_decode to shutdown */
   pthread_mutex_lock(&pipe->video_decode.eos_mutex);
   while (!pipe->video_decode.eos)
     pthread_cond_wait(&pipe->video_decode.eos_cv,&pipe->video_decode.eos_mutex);
   pthread_mutex_unlock(&pipe->video_decode.eos_mutex);
#endif
         
   DEBUGF("[vcodec] omx_teardown pipeline 1\n");

   /* Transition all components to Idle, if they have been initialised */
   omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateIdle, NULL); 
   omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateIdle, NULL);
   DEBUGF("pipe->do_deinterlace=%d, pipe->image_fx=%d\n",pipe->do_deinterlace,(int)pipe->image_fx.h);
   if (pipe->video_decode.port_settings_changed == 2) {
      if (pipe->do_deinterlace) { 
        omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateIdle, NULL); 
      } else {
        omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateIdle, NULL); 
      }
   }
   if (pipe->image_fx.port_settings_changed == 2) {
     omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateIdle, NULL);
   }
   if (pipe->video_scheduler.port_settings_changed == 2) {
     omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
   }
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateIdle, NULL);

#if 0
   DEBUGF("[vcodec] omx_teardown pipeline 2\n");
   /* Wait for video_render to shutdown */
   pthread_mutex_lock(&pipe->video_render.eos_mutex);
   while (!pipe->video_render.eos)
     pthread_cond_wait(&pipe->video_render.eos_cv,&pipe->video_render.eos_mutex);
   pthread_mutex_unlock(&pipe->video_render.eos_mutex);
#endif

/* 
  Pipeline is as follows:

[video data] -> 130 video_decode 131 -> 190 image_fx 191 -> 10 video_scheduler 11 -> 90 video_render
                                                clock 81 -> 12 video_scheduler
                                                clock 80 -> 101 audio_render
                                            [audio data] -> 100 audio_render
*/

   /* Flush entrances to pipeline */
   omx_send_command_and_wait(&pipe->video_decode,OMX_CommandFlush,130,NULL);
   omx_send_command_and_wait(&pipe->audio_render,OMX_CommandFlush,100,NULL);

   /* Flush all tunnels */
   DEBUGF("[vcodec] omx_teardown pipeline 3\n");
   if (pipe->do_deinterlace) {
     omx_flush_tunnel(&pipe->video_decode, 131, &pipe->image_fx, 190);
     omx_flush_tunnel(&pipe->image_fx, 191, &pipe->video_scheduler, 10);
   } else {
     omx_flush_tunnel(&pipe->video_decode, 131, &pipe->video_scheduler, 10);
   }
   DEBUGF("[vcodec] omx_teardown pipeline 4\n");
   omx_flush_tunnel(&pipe->video_scheduler, 11, &pipe->video_render, 90);
   omx_flush_tunnel(&pipe->clock, 81, &pipe->video_scheduler, 12);

   DEBUGF("[vcodec] omx_teardown pipeline 2b\n");

   omx_send_command_and_wait(&pipe->video_scheduler,OMX_CommandFlush,10,NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 5\n");

   omx_flush_tunnel(&pipe->clock, 80, &pipe->audio_render, 101);

   /* Disable audio_render input port and buffers */
   omx_send_command_and_wait0(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);
   omx_free_buffers(&pipe->audio_render, 100);
   omx_send_command_and_wait1(&pipe->audio_render, OMX_CommandPortDisable, 100, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 9\n");

   /* Scheduler -> render tunnel */
   if (pipe->video_scheduler.port_settings_changed == 2) {
     omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 11, NULL);
     omx_send_command_and_wait(&pipe->video_render, OMX_CommandPortDisable, 90, NULL);

     omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 10, NULL);
   }

   if ((pipe->image_fx.port_settings_changed == 2) && (pipe->do_deinterlace)) {
     omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortDisable, 190, NULL);
     omx_send_command_and_wait(&pipe->image_fx, OMX_CommandPortDisable, 191, NULL);
   }

   DEBUGF("[vcodec] omx_teardown pipeline 8a\n");

   //dumpport(pipe->video_scheduler.h,10);

   /* Teardown tunnels */
/* 
  Pipeline is as follows:

[video data] -> 130 video_decode 131 -> 190 image_fx 191 -> 10 video_scheduler 11 -> 90 video_render
                                                clock 81 -> 12 video_scheduler
                                                clock 80 -> 101 audio_render
                                            [audio data] -> 100 audio_render
*/
   //dumpport(pipe->video_decode.h,131);
   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 10, NULL, 0));

   DEBUGF("[vcodec] omx_teardown pipeline 10\n");

   /* NOTE: The clock disable doesn't complete until after the video scheduler port is 
      disabled (but it completes before the video scheduler port disabling completes). */
   OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortDisable, 80, NULL));
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandPortDisable, 101, NULL);
   OERR(OMX_SendCommand(pipe->clock.h, OMX_CommandPortDisable, 81, NULL));
   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandPortDisable, 12, NULL);

   DEBUGF("[vcodec] omx_teardown pipeline 12b\n");

   if (pipe->do_deinterlace) {
     OERR(OMX_SetupTunnel(pipe->image_fx.h, 190, NULL, 0));
     OERR(OMX_SetupTunnel(pipe->image_fx.h, 191, NULL, 0));
   }

   DEBUGF("[vcodec] omx_teardown pipeline 13\n");

   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 11, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->video_render.h, 90, NULL, 0));

   OERR(OMX_SetupTunnel(pipe->clock.h, 81, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->video_scheduler.h, 12, NULL, 0));

   DEBUGF("[vcodec] omx_teardown pipeline 13b\n");

   OERR(OMX_SetupTunnel(pipe->clock.h, 80, NULL, 0));
   OERR(OMX_SetupTunnel(pipe->audio_render.h, 101, NULL, 0));

   DEBUGF("[vcodec] omx_teardown pipeline 8b\n");


/* 
  Pipeline is as follows:

[video data] -> 130 video_decode 131 -> 190 image_fx 191 -> 10 video_scheduler 11 -> 90 video_render
                                                clock 81 -> 12 video_scheduler
                                                clock 80 -> 101 audio_render
                                            [audio data] -> 100 audio_render
*/

   omx_show_state(&pipe->video_decode,130,131,0);
   dumpport(pipe->video_decode.h,131);
   omx_show_state(&pipe->video_scheduler,10,11,12);
   if (pipe->do_deinterlace) { omx_show_state(&pipe->image_fx,190,191,0); }
   omx_show_state(&pipe->video_render,90,0,0);
   omx_show_state(&pipe->audio_render,100,101,0);
   omx_show_state(&pipe->clock,80,81,0);

   if (pipe->video_decode.port_settings_changed == 2) {
     //dumpport(pipe->video_decode.h,131);
     omx_send_command_and_wait(&pipe->video_decode, OMX_CommandPortDisable, 131, NULL);
   }

   DEBUGF("[vcodec] omx_teardown pipeline 11\n");

   /* Disable video_decode input port and buffers */
   //dumpport(pipe->video_decode.h,130);
   omx_send_command_and_wait0(&pipe->video_decode, OMX_CommandPortDisable, 130, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 6\n");
   omx_free_buffers(&pipe->video_decode, 130);
   DEBUGF("[vcodec] omx_teardown pipeline 7\n");
   //omx_send_command_and_wait1(&pipe->video_decode, OMX_CommandPortDisable, 130, NULL);

   //dumpport(pipe->video_decode.h,130);
   if (is_port_enabled(pipe->video_decode.h, 130)) {
     fprintf(stderr,"Unexpected error video_decode port 130 is not disabled\n");
     exit(1);
   }

   DEBUGF("[vcodec] omx_teardown pipeline 12\n");

   OERR(OMX_SetupTunnel(pipe->video_decode.h, 131, NULL, 0));

   DEBUGF("[vcodec] omx_teardown pipeline 15\n");

   omx_show_state(&pipe->video_decode,130,131,0);

   /* Transition all components to Loaded */
   DEBUGF("[vcodec] omx_teardown pipeline 15a\n");
   omx_send_command_and_wait(&pipe->video_decode, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 15b\n");
   omx_send_command_and_wait(&pipe->video_scheduler, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 15c\n");
   if (((pipe->video_decode.port_settings_changed == 2) && (pipe->do_deinterlace)) || (pipe->image_fx.port_settings_changed == 2)) {
     omx_send_command_and_wait(&pipe->video_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   }
   DEBUGF("[vcodec] omx_teardown pipeline 15d\n");
   omx_send_command_and_wait(&pipe->audio_render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 15e\n");
   omx_send_command_and_wait(&pipe->clock, OMX_CommandStateSet, OMX_StateLoaded, NULL);
   DEBUGF("[vcodec] omx_teardown pipeline 15f\n");
   if (pipe->do_deinterlace) { omx_send_command_and_wait(&pipe->image_fx, OMX_CommandStateSet, OMX_StateLoaded, NULL); }

   DEBUGF("[vcodec] omx_teardown pipeline 16\n");
   /* Finally free the component handles */
   OERR(OMX_FreeHandle(pipe->video_decode.h));
   OERR(OMX_FreeHandle(pipe->video_scheduler.h));
   OERR(OMX_FreeHandle(pipe->video_render.h));
   OERR(OMX_FreeHandle(pipe->audio_render.h));
   OERR(OMX_FreeHandle(pipe->clock.h));
   if (pipe->do_deinterlace) { OERR(OMX_FreeHandle(pipe->image_fx.h)); }
   DEBUGF("[vcodec] omx_teardown pipeline 17\n");
}
