#ifndef _OSD_VIEW_H
#define _OSD_VIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "osd.h"
#include "osd_model.h"
#include "events.h"

#define OSD_XMARGIN 32
#define OSD_YMARGIN 18

#define PADDING_X 15
#define PADDING_Y 20

/* constans for channellist */
#define CHANNELLIST_TEXTSIZE 40
#define COLOR_TEXT GRAPHICS_RGBA32(0xff,0xff,0xff,0xff)
#define COLOR_SELECTED_TEXT GRAPHICS_RGBA32(0x00,0xff,0xff,0xff)
#define COLOR_BLUE GRAPHICS_RGBA32(0xff,0x00,0x00,0xff)
#define COLOR_BACKGROUND GRAPHICS_RGBA32(0,0,0,0x80)
#define COLOR_SELECTED_BACKGROUND GRAPHICS_RGBA32(0xff,0,0,0x80)

void osd_view(struct osd_t* osd, int view);
#endif
