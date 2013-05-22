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
#include "codec.h"

#define SCREEN 0
#define BG_LAYER 0
#define FG_LAYER 2
#define OSD_LAYER 3

#define OSD_XMARGIN 32
#define OSD_YMARGIN 18

double get_time(void)
{
  struct timeval tv;

  gettimeofday(&tv,NULL);

  double x = tv.tv_sec;
  x *= 1000;
  x += tv.tv_usec / 1000;

  return x;
}

static void utf8decode(char* str, char* r)
{
  int x,y,z,ud;
  char* p = str;

  while ((z = *p++)) {
    if (z < 128) {
      *r++ = z;
    } else {
      y=*p++;
      if (y==0) { *r=0 ; return; } // ERROR
      if (z < 224) {
        ud=(z-192)*64 + (y-128);
      } else {
        x=*p++;
        if (x==0) { *r=0 ; return; } // ERROR
        ud=(z-224)*4096 + (y-128)*64 + (x-128);
      }

      if (ud < 256) {
        *r++ = ud;
      } else {
        /* Transliterate some common characters */
        switch (ud) {  
          /* Add more mappings here if required  */
          case 0x201c:                              // quotedblleft
          case 0x201d: *r++ = '"';  break;          // quotedblright
          case 0x2018:                              // quoteleft
          case 0x2019: *r++ = '\''; break;          // quoteright
          case 0x2013:                              // en dash
          case 0x2014: *r++ = '-'; break;           // em dash
          case 0x20ac: *r++ = 0xa4; break;          // euro
          case 0x27a2:                              // square
          case 0x25ca:                              // diamond
          case 0xf076:
          case 0xf0a7:
          case 0xf0b7:
          case 0x2022: *r++ = '\267'; break;        // bullet ("MIDDLE DOT" in iso-8859-1)
          case 0x2026: fprintf(stdout,"..."); break;  // ellipsis

          default:     //fprintf(stderr,"Unknown character %04x (UTF-8 is %02x %02x %02x)\n",ud,z,y,x);
          *r++ = ' ';   
           break;
        }
      }
    }
  }

  *r++ = 0;
  return;
}

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
   fprintf(stderr,"Display width=%d, height=%d\n",display_width,display_height);

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
   osd->osd_cleartime = 0.0;

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

void osd_show_channellist(struct osd_t* osd, struct channel_t* p)
{
   int32_t s=0;
   uint32_t width,height;
   uint32_t y_offset = OSD_YMARGIN;
   uint32_t x_offset = OSD_XMARGIN;
   uint32_t curr_offset = y_offset+80;
   uint32_t text_size = 40;
   const char *text = "Channel Listing";
   char ch_text[100];
   int tmp_offset = 0;
   uint32_t text_length = strlen(text);

   fprintf(stderr,"osd_show_channellist\n");

   height = osd->display_height;
   width = osd->display_width / 3;

   graphics_resource_fill(osd->img, x_offset, y_offset, width, height, GRAPHICS_RGBA32(0x173,0x216,0x230,0x80));
   graphics_resource_fill(osd->img, x_offset, y_offset, width, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset, y_offset+height-2, width, 2, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset, y_offset, 2, height, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));
   graphics_resource_fill(osd->img, x_offset+width-2, y_offset, 2, height, GRAPHICS_RGBA32(0xff,0xff,0xff,0xa0));

   s = graphics_resource_render_text_ext(osd->img, x_offset+50, y_offset+25,
                                     width,
                                     50,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0x173,0x216,0x230,0x80), /* bg */
                                     text, text_length, text_size);
  if (channellist_offset > 0) {
      fprintf(stderr,"channellist_offset %d\n",channellist_offset);
      while (tmp_offset != channellist_offset) {
        p = p->next;
        tmp_offset += 1;
      };
  };
  while (p) {
    fprintf(stderr,"chan: %5d  %5d - %s\n",p->id,p->lcn,p->name);
    snprintf(ch_text,sizeof(ch_text),"%5d - %s",p->lcn,p->name);
    fprintf(stderr,"Now rendering '%s' at offset %d\n",ch_text,curr_offset);
       s = graphics_resource_render_text_ext(osd->img, x_offset+50, curr_offset,
                                     width,
                                     50,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     ch_text, strlen(ch_text), text_size);
    p = p->next;
    curr_offset += 65;
    if (curr_offset > 978) {
      fprintf(stderr,"Next page needed at offset %d\n",curr_offset);
      break;
    };
  }

  pthread_mutex_lock(&osd->osd_mutex);
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);
  osd->osd_cleartime = get_time() + 20000;
  osd->osd_state = OSD_CHANNELLIST;
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

void osd_alert(struct osd_t* osd, char* text)
{
  uint32_t text_length;
  int32_t s=0;
  uint32_t width,height;
  uint32_t y_offset;
  uint32_t x_offset;
  uint32_t text_size = 40;

  pthread_mutex_lock(&osd->osd_mutex);

  /* TODO: Only clear alert area */
  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));

  if (text) {
    fprintf(stderr,"[OSD ALERT]: %s\n",text);
    text_length = strlen(text);
    s = graphics_resource_text_dimensions_ext(osd->img, text, text_length, &width, &height, text_size);

    x_offset = ((1920 - width) / 2);
    y_offset = (1080 - height) / 2;

    osd_draw_window(osd,x_offset,y_offset,width+100,height+50);

    s = graphics_resource_render_text_ext(osd->img, x_offset+50, y_offset+25,
                                          width,
                                          height,
                                          GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                          GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                          text, text_length, text_size);
  }

  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);

  pthread_mutex_unlock(&osd->osd_mutex);
}

static void osd_show_eventinfo(struct osd_t* osd, struct event_t* event, struct event_t* nextEvent)
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

  /* Start/stop time - current event */
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

  /* Title - current event */
  if (event->title) {
    char* iso_text = malloc(strlen(event->title)+1);
    utf8decode(event->title,iso_text);
    s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+350, 720,
                                       width,
                                       height,
                                       GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                       GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                       iso_text, strlen(iso_text), 40);
    free(iso_text);
  }

  snprintf(str,sizeof(str),"%dh %02dm",duration/3600,(duration%3600)/60);
  s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 800,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 30);

  if ((event->episodeNumber) || (event->seasonNumber)) {
    if (!event->episodeNumber) {
      snprintf(str,sizeof(str),"Season %d",event->seasonNumber);
    } else if (!event->seasonNumber) {
      snprintf(str,sizeof(str),"Episode %d",event->episodeNumber);
    } else {
      snprintf(str,sizeof(str),"Season %d, Ep. %d",event->seasonNumber,event->episodeNumber);
    }
    s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 838,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 30);
  }

  if (event->description) {
    char* iso_text = malloc(strlen(event->description)+1);
    utf8decode(event->description,iso_text);
    render_paragraph(osd->img,iso_text,30,OSD_XMARGIN+350,800);
    free(iso_text);
  }


  if (nextEvent) {
    osd_draw_window(osd,OSD_XMARGIN,1002,width,78-OSD_YMARGIN);
    /* Start/stop time - next event */
    localtime_r((time_t*)&nextEvent->start,&start_time);
    localtime_r((time_t*)&nextEvent->stop,&stop_time);

    snprintf(str,sizeof(str),"%02d:%02d - %02d:%02d",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min);
    s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+50, 1020,
                                       width,
                                       height,
                                       GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                       GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 40);


    if (nextEvent->title) {
      char* iso_text = malloc(strlen(nextEvent->title)+1);
      utf8decode(nextEvent->title,iso_text);
      s = graphics_resource_render_text_ext(osd->img, OSD_XMARGIN+350, 1020,
                                         width,
                                         height,
                                         GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                         GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                         iso_text, strlen(iso_text), 40);
      free(iso_text);
    }
  }


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
  int width = 218;
  int height = 80;

  osd_draw_window(osd,1670,18,width,height);

  now = time(NULL);
  localtime_r(&now,&now_tm);

  snprintf(str,sizeof(str),"%02d:%02d.%02d",now_tm.tm_hour,now_tm.tm_min,now_tm.tm_sec);

  osd->last_now = now;

  s = graphics_resource_render_text_ext(osd->img, 1700, OSD_YMARGIN+25,
                                     width,
                                     height,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
				     str, strlen(str), 40);
}

void osd_show_info(struct osd_t* osd, int channel_id, int timeout)
{
  char str[128];
  
  osd->event = channels_geteventid(channel_id);
  osd->nextEvent = channels_getnexteventid(channel_id);

  struct event_t* event = event_copy(osd->event);
  struct event_t* nextEvent = event_copy(osd->nextEvent);

  fprintf(stderr,"***OSD: event=%d\n",(event ? event->eventId : -1));
  event_dump(event);
  fprintf(stderr,"***OSD: nextEvent=%d\n",(nextEvent ? nextEvent->eventId : -1));
  event_dump(nextEvent);
  fprintf(stderr,"******\n");
  snprintf(str,sizeof(str),"%03d - %s",channels_getlcn(channel_id),channels_getname(channel_id));
  char* iso_text = malloc(strlen(str)+1);
  utf8decode(str,iso_text);

  pthread_mutex_lock(&osd->osd_mutex);
  osd_show_channelname(osd,iso_text);

  osd_show_time(osd);

  osd_show_eventinfo(osd,event,nextEvent);

  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);

  free(iso_text);

  osd->osd_state = OSD_INFO;
  if (timeout) {
    osd->osd_cleartime = get_time() + timeout;
  }

  event_free(event);
  event_free(nextEvent);
}

void osd_show_newchannel(struct osd_t* osd, int channel)
{
  char str[128];

  if ((osd->osd_state != OSD_NONE) && (osd->osd_state != OSD_NEWCHANNEL)) {
    osd_clear(osd);
  }

  osd->osd_state = OSD_NEWCHANNEL;

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

void osd_show_audio_menu(struct osd_t* osd, struct codecs_t* codecs, int audio_stream)
{
  /* Only display to console for now */
  int i;

  for (i=0;i<codecs->subscription.numstreams;i++) {
    struct htsp_stream_t *stream = &codecs->subscription.streams[i];
    if (stream->type == HMF_STREAM_AUDIO) {
      if (i==audio_stream) {
        fprintf(stderr,"*** ");
      }
      fprintf(stderr,"Stream %d, codec %d, lang %s, type=%d\n",i,stream->codec,stream->lang,stream->audio_type);
    }
  }
}

void osd_clear(struct osd_t* osd)
{
  pthread_mutex_lock(&osd->osd_mutex);
  graphics_resource_fill(osd->img, 0, 0, osd->display_width, osd->display_height, GRAPHICS_RGBA32(0,0,0,0));
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
  pthread_mutex_unlock(&osd->osd_mutex);

  fprintf(stderr,"Clearing OSD...\n");

  osd->osd_state = OSD_NONE;
  osd->osd_cleartime = 0;
}

void osd_update(struct osd_t* osd, int channel_id)
{
  if ((osd->osd_cleartime) && (get_time() > osd->osd_cleartime)) {
    osd_clear(osd);
    return;
  }

  if (osd->osd_state == OSD_INFO) {
    time_t now = time(NULL);
    if (now != osd->last_now) {
      osd_show_time(osd);
      graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);
    }

    if (osd->event != channels_geteventid(channel_id)) {
      osd_show_info(osd, channel_id, 0);
    }
  }
}

int osd_process_key(struct osd_t* osd, int c) {
/* process and check keypresses whilst osd shown */

  if (osd->osd_state == OSD_NONE) return c;

  if (osd->osd_state == OSD_CHANNELLIST) {
    switch (c) {
      case 'n':
        fprintf(stderr,"OSD key: n pressed -- next page (%d)\n",channellist_offset);
        channellist_offset += 3;
        fprintf(stderr,"OSD page (%d)\n",channellist_offset);
        osd_show_channellist(osd, channels_return_struct());
        return -1;
        break;
      case 'p':
        fprintf(stderr,"OSD key: p pressed -- previous page\n");
        if (channellist_offset > 3) {
          channellist_offset -= 3;
        } else {
          channellist_offset = 0;
        };
        osd_show_channellist(osd, channels_return_struct());
        return -1;
        break;
    };
  };
return c;
}
