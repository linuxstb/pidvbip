#ifndef _VO_PI_H
#define _VO_PI_H

#include <bcm_host.h>

#define VO_ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))

typedef struct
{
  DISPMANX_DISPLAY_HANDLE_T   display;
  DISPMANX_MODEINFO_T         info;
  void                       *image;
  DISPMANX_UPDATE_HANDLE_T    update;
  DISPMANX_RESOURCE_HANDLE_T  resource;
  DISPMANX_ELEMENT_HANDLE_T   element;
  uint32_t                    vc_image_ptr;

} RECT_VARS_T;

void vo_open(RECT_VARS_T* vars, int screen);

void vo_display_frame (RECT_VARS_T* vars, int width, int height,
                      int chroma_width, int chroma_height,
                      uint8_t * const * buf, int num);

void vo_close(RECT_VARS_T* vars);

#endif
