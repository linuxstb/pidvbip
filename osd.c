
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "vgfont.h"
#include "bcm_host.h"
#include "tiresias_pcfont.h"
#include "osd.h"

#define SCREEN 0
#define LAYER 1

void osd_init(struct osd_t* osd)
{
   uint32_t display_width, display_height;
   int s;

   s = gx_graphics_init(tiresias_pcfont,sizeof(tiresias_pcfont));
   assert(s == 0);

   s = graphics_get_display_size(0, &display_width, &display_height);
   assert(s == 0);
   fprintf(stderr,"Display width=%d, height=%d\n",display_width,display_height);

   s = gx_create_window(SCREEN, display_width-100, 100, GRAPHICS_RESOURCE_RGBA32, &osd->img);
   assert(s == 0);
   graphics_resource_fill(osd->img, 0, 0, display_width, 100, GRAPHICS_RGBA32(0,0,0,0));

   graphics_display_resource(osd->img, 0, LAYER, 50, 30, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);
}

void osd_done(struct osd_t* osd)
{
   graphics_display_resource(osd->img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(osd->img);
}


void osd_show_channelname(struct osd_t* osd, const char *text)
{
   uint32_t text_length = strlen(text);
   int32_t s=0;
   uint32_t width,height;
   uint32_t y_offset = 20;
   uint32_t x_offset = 0;
   uint32_t text_size = 40;

   s = graphics_resource_text_dimensions_ext(osd->img, text, text_length, &width, &height, text_size);

   width = 600;

   graphics_resource_fill(osd->img, x_offset, y_offset, width+100, height+60, GRAPHICS_RGBA32(0,0,0,0x80));

   s = graphics_resource_render_text_ext(osd->img, x_offset+50, y_offset+25,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     text, text_length, text_size);

   graphics_resource_fill(osd->img, x_offset, y_offset, width+100, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset, y_offset+height+28, width+100, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset, y_offset, 2, height+28, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset+width+98, y_offset, 2, height+28, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));

   graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);

   return s;
}

