
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include "vgfont.h"
#include "bcm_host.h"
#include "tiresias_pcfont.h"
#include "osd.h"
#include "channels.h"
#include "events.h"

#define SCREEN 0
#define LAYER 1

#define OSD_XMARGIN 32
#define OSD_YMARGIN 18

static const char *strnchr(const char *str, size_t len, char c)
{
   const char *e = str + len;
   do {
      if (*str == c) {
         return str;
      }
   } while (++str < e);
   return NULL;
}

int32_t render_paragraph(GRAPHICS_RESOURCE_HANDLE img, const char *text, const int skip, const uint32_t text_size, const uint32_t y_offset)
{
   uint32_t text_length = strlen(text)-skip;
   uint32_t width=0, height=0;
   const char *split = text;
   int32_t s=0;
   int len = 0; // length of pre-paragraph
   uint32_t img_w, img_h;

   img_w = 1400;
   img_h = 300;
   //graphics_get_resource_size(img, &img_w, &img_h);

   if (text_length==0)
      return 0;
   while (split[0]) {
      s = graphics_resource_text_dimensions_ext(img, split, text_length-(split-text), &width, &height, text_size);
      if (s != 0) return s;
      if (width > img_w) {
         const char *space = strnchr(split, text_length-(split-text), ' ');
         if (!space) {
            len = split+1-text;
            split = split+1;
         } else {
            len = space-text;
            split = space+1;
         }
      } else {
         break;
      }
   }
   // split now points to last line of text. split-text = length of initial text. text_length-(split-text) is length of last line
   if (width) {
      s = graphics_resource_render_text_ext(img, OSD_XMARGIN+350, y_offset-height,
                                     GRAPHICS_RESOURCE_WIDTH,
                                     GRAPHICS_RESOURCE_HEIGHT,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     split, text_length-(split-text), text_size);
      if (s!=0) return s;
   }
   return render_paragraph(img, text, skip+text_length-len, text_size, y_offset - height);
}


void osd_init(struct osd_t* osd)
{
   uint32_t display_width, display_height;
   int s;

   s = gx_graphics_init(tiresias_pcfont,sizeof(tiresias_pcfont));
   assert(s == 0);

   s = graphics_get_display_size(0, &display_width, &display_height);
   osd->display_width = display_width;
   osd->display_height = display_height;

   assert(s == 0);
   //fprintf(stderr,"Display width=%d, height=%d\n",display_width,display_height);

   s = gx_create_window(SCREEN, display_width, display_height, GRAPHICS_RESOURCE_RGBA32, &osd->img);
   assert(s == 0);
   graphics_resource_fill(osd->img, 0, 0, display_width, display_height, GRAPHICS_RGBA32(0,0,0,0));

   graphics_display_resource(osd->img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);
}

void osd_done(struct osd_t* osd)
{
   graphics_display_resource(osd->img, 0, LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(osd->img);
}


static void osd_draw_window(struct osd_t* osd, int x, int y, int width, int height)
{
   graphics_resource_fill(osd->img, x, y, width, height, GRAPHICS_RGBA32(0,0,0,0x80));

   graphics_resource_fill(osd->img, x, y, width, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x, y+height-2, width, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x, y, 2, height, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x+width-2, y, 2, height, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
}

static void osd_show_channelname(struct osd_t* osd, const char *text)
{
   uint32_t text_length = strlen(text);
   int32_t s=0;
   uint32_t width,height;
   uint32_t y_offset = OSD_YMARGIN;
   uint32_t x_offset = OSD_XMARGIN;
   uint32_t text_size = 40;

   //s = graphics_resource_text_dimensions_ext(osd->img, text, text_length, &width, &height, text_size);

   height = 80;
   width = 600;

   osd_draw_window(osd,x_offset,y_offset,width,height);

   s = graphics_resource_render_text_ext(osd->img, x_offset+50, y_offset+25,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     text, text_length, text_size);

}

static void osd_show_eventinfo(struct osd_t* osd, struct event_t* event)
{
  char str[64];
  int s;
  struct tm start_time;
  struct tm stop_time;
  int duration = event->stop - event->start;
  int width = 1920-2*OSD_XMARGIN;
  int height = 380-OSD_YMARGIN;

  localtime_r((time_t*)&event->start,&start_time);
  localtime_r((time_t*)&event->stop,&stop_time);

  osd_draw_window(osd,OSD_XMARGIN,700,width,height);

  snprintf(str,sizeof(str),"%dh %02dm",duration/3600,(duration%3600)/60);

  s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 800,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 30);


  snprintf(str,sizeof(str),"%02d:%02d - %02d:%02d",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min);

  s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 720,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 40);

  s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+350, 720,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     event->title, strlen(event->title), 40);


  render_paragraph(osd->img, event->description,0,30,900);

  //fprintf(stderr,"Title:       %s\n",event->title);
  //fprintf(stderr,"Start:       %04d-%02d-%02d %02d:%02d:%02d\n",start_time.tm_year+1900,start_time.tm_mon+1,start_time.tm_mday,start_time.tm_hour,start_time.tm_min,start_time.tm_sec);
  //fprintf(stderr,"Stop:        %04d-%02d-%02d %02d:%02d:%02d\n",stop_time.tm_year+1900,stop_time.tm_mon+1,stop_time.tm_mday,stop_time.tm_hour,stop_time.tm_min,stop_time.tm_sec);
  //fprintf(stderr,"Duration:    %02d:%02d:%02d\n",
  //fprintf(stderr,"Description: %s\n",event->description);
}

static void osd_show_time(struct osd_t* osd)
{
  struct tm now_tm;
  time_t now;
  char str[32];
  int s;
  int width = 188;
  int height = 80;

  osd_draw_window(osd,1700,18,width,height);

  now = time(NULL);
  localtime_r(&now,&now_tm);

  snprintf(str,sizeof(str),"%02d:%02d",now_tm.tm_hour,now_tm.tm_min);

  s = graphics_resource_render_text_ext(osd->img, 1730, OSD_YMARGIN+25,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 40);
}

void osd_show_info(struct osd_t* osd, int channel_id)
{
  char str[128];

  struct event_t* event = event_copy(channels_geteventid(channel_id));

  event_dump(event);

  snprintf(str,sizeof(str),"%03d - %s",channels_getlcn(channel_id),channels_getname(channel_id));
  osd_show_channelname(osd,str);

  osd_show_time(osd);

  osd_show_eventinfo(osd,event);

  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);

  event_free(event);
}

void osd_show_newchannel(struct osd_t* osd, int channel)
{
  char str[128];

  snprintf(str,sizeof(str),"%d",channel);
  if (channel < 1000) {
    str[4] = 0;
    str[3] = '-';
    if (channel < 100) {
      str[2] = '-';
      if (channel < 10) {
        str[1] = '-';
      }
    }
  }
  fprintf(stderr,"New channel = %s\n",str);
  osd_show_channelname(osd,str);
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
}

void osd_clear_newchannel(struct osd_t* osd)
{
  /* TODO: Only clear channel area */

  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);

  fprintf(stderr,"Clearing OSD...\n");
}

void osd_clear(struct osd_t* osd)
{
  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);

  fprintf(stderr,"Clearing OSD...\n");
}
