#ifndef _OSD_H
#define _OSD_H

#include "vgfont.h"

struct osd_t {
  GRAPHICS_RESOURCE_HANDLE img_bg;
  GRAPHICS_RESOURCE_HANDLE img;
};

void osd_init(struct osd_t* osd);
void osd_done(struct osd_t* osd);
void osd_show_channelname(struct osd_t* osd, const char *text);

#endif
