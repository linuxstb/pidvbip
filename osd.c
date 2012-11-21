
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include <bcm_host.h>
#include "libs/vgfont/vgfont.h"
#include "tiresias_pcfont.h"
#include "osd.h"
#include "channels.h"
#include "events.h"

#define SCREEN 0
#define BG_LAYER 0
#define FG_LAYER 2
#define OSD_LAYER 3

#define OSD_XMARGIN 32
#define OSD_YMARGIN 18

int32_t render_paragraph(GRAPHICS_RESOURCE_HANDLE img, const char *text, const uint32_t text_size, const uint32_t x_offset, const uint32_t y_offset)
{
   uint32_t text_length;
   uint32_t line_length;
   uint32_t width=0, height=0;
   const char *split = text;
   int32_t s=0;
   uint32_t img_w = 1400;;

   if ((!text) || ((text_length=strlen(text))==0))
      return 0;

   //fprintf(stderr,"render_paragraph(\"%s\",%d)\n",text,text_length);

   s = graphics_resource_text_dimensions_ext(img, text, text_length, &width, &height, text_size);
   if (s != 0) return s;

   if (width <= img_w) {
     /* We can display the whole line */
     line_length = text_length;
   } else {
     //fprintf(stderr,"width=%d, img_w=%d, looking for next space\n",width,img_w);

     const char* space = index(split,' ');

     if (space) {
       s = graphics_resource_text_dimensions_ext(img, text, space-text, &width, &height, text_size);
       if (s != 0) return s;
     }

     if ((space == NULL) || (width > img_w)) {
       /* No spaces, within img_w. Just go through character by character */
       line_length = 0;
       do {
         line_length++;
         s = graphics_resource_text_dimensions_ext(img, text, text_length, &width, &height, text_size);
         if (s != 0) return s;
       } while (width < img_w);

       line_length--;
     } else {
       /* We have at least one space, so can split line on a space */
       width = 0;
       line_length = space - text;

       while (width < img_w) {
         space = index(space+1,' ');
         s = graphics_resource_text_dimensions_ext(img, text, space - text, &width, &height, text_size);
         if (s != 0) return s;

         if (width < img_w) { line_length = space - text; }
       }
     }
   }

   if (line_length) {
     //int i;
     //fprintf(stderr,"Rendering: ");
     //for (i=0;i<line_length;i++) { fprintf(stderr,"%c",text[i]); }
     //fprintf(stderr,"\n");

     s = graphics_resource_render_text_ext(img, x_offset, y_offset,
                                     GRAPHICS_RESOURCE_WIDTH,
                                     GRAPHICS_RESOURCE_HEIGHT,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     text, line_length, text_size);
      if (s!=0) return s;
   }
   if (text[line_length]) {
     return render_paragraph(img, text + line_length+1, text_size, x_offset, y_offset + height);
   } else {
     return 0;
   }
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

   /* The main OSD image */
   s = gx_create_window(SCREEN, display_width, display_height, GRAPHICS_RESOURCE_RGBA32, &osd->img);
   assert(s == 0);
   graphics_resource_fill(osd->img, 0, 0, display_width, display_height, GRAPHICS_RGBA32(0,0,0,0));

   graphics_display_resource(osd->img, 0, OSD_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);

   /* A full-screen black image to either remove any left-over console text (BG_LAYER) or to hide the video (FG_LAYER) */
   s = gx_create_window(SCREEN, display_width, display_height, GRAPHICS_RESOURCE_RGBA32, &osd->img_blank);
   assert(s == 0);
   graphics_resource_fill(osd->img_blank, 0, 0, display_width, display_height, GRAPHICS_RGBA32(0,0,0,255));

   graphics_display_resource(osd->img_blank, 0, BG_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);

   osd->video_blanked = 0;

   pthread_mutex_init(&osd->osd_mutex,NULL);
}

void osd_blank_video(struct osd_t* osd, int on_off)
{
  pthread_mutex_lock(&osd->osd_mutex);
  if (on_off) {
    /* Display on top of video */
    graphics_display_resource(osd->img_blank, 0, FG_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);
  } else {
    /* Display underneath video */
    graphics_display_resource(osd->img_blank, 0, BG_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 1);
  }
  pthread_mutex_unlock(&osd->osd_mutex);
}

void osd_done(struct osd_t* osd)
{
   graphics_display_resource(osd->img, 0, OSD_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(osd->img);

   graphics_display_resource(osd->img_blank, 0, BG_LAYER, 0, 0, GRAPHICS_RESOURCE_WIDTH, GRAPHICS_RESOURCE_HEIGHT, VC_DISPMAN_ROT0, 0);
   graphics_delete_resource(osd->img_blank);
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
  int duration;
  int width = 1920-2*OSD_XMARGIN;
  int height = 380-OSD_YMARGIN;

  osd_draw_window(osd,OSD_XMARGIN,700,width,height);

  if (event==NULL)
    return;

  localtime_r((time_t*)&event->start,&start_time);
  localtime_r((time_t*)&event->stop,&stop_time);
  duration = event->stop - event->start;

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


  snprintf(str,sizeof(str),"%dh %02dm",duration/3600,(duration%3600)/60);
  s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 800,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 30);


  render_paragraph(osd->img, event->description,30,OSD_XMARGIN+350,800);

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

  pthread_mutex_lock(&osd->osd_mutex);
  osd_show_channelname(osd,str);

  osd_show_time(osd);

  osd_show_eventinfo(osd,event);

  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);

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
  pthread_mutex_lock(&osd->osd_mutex);
  osd_show_channelname(osd,str);
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);
}

void osd_clear_newchannel(struct osd_t* osd)
{
  /* TODO: Only clear channel area */

  pthread_mutex_lock(&osd->osd_mutex);
  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);

  fprintf(stderr,"Clearing OSD...\n");
}

void osd_clear(struct osd_t* osd)
{
  pthread_mutex_lock(&osd->osd_mutex);
  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);

  fprintf(stderr,"Clearing OSD...\n");
}
