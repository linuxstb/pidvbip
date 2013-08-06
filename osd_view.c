/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013
(C) Daniel Nordqvist 2013

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

#include "osd_view.h"

static uint32_t channellist_win_x; 
static uint32_t channellist_win_y;
static uint32_t channellist_win_w;
static uint32_t channellist_win_h;

static uint32_t nowandnext_win_x;
static uint32_t nowandnext_win_y;
static uint32_t nowandnext_win_w;
static uint32_t nowandnext_win_h;

static uint32_t eventinfo_win_x;
static uint32_t eventinfo_win_y;
static uint32_t eventinfo_win_w;
static uint32_t eventinfo_win_h;

/* 
 * Display the channels in the channellist window
 */
static void osd_channellist_channels(struct osd_t* osd)
{
  int i;
  char str[60];
  uint32_t row_space_y = 50;
  uint32_t x = channellist_win_x + PADDING_X;
  uint32_t y = channellist_win_y + PADDING_Y;
  uint32_t color;
  uint32_t bg_color;
  
  for (i = 0; i < osd->model_channellist.numUsed; i++) {
    if ( compareIndexModelChannelList(&osd->model_channellist, &osd->model_channellist_current, i) == 1 ) {
      printf("osd_channellist_channels: Update index %d - lcn %d\n", i, osd->model_channellist.channel[i].lcn);
      if (osd->model_channellist.selectedIndex == i) {
        color = COLOR_SELECTED_TEXT;
        if (osd->model_channellist.active) {
          bg_color = COLOR_SELECTED_BACKGROUND;
        }
        else {
          bg_color = COLOR_BACKGROUND;
        }
      }
      else {
        color = COLOR_TEXT;
        bg_color = COLOR_BACKGROUND;
      }
      snprintf(str, sizeof(str), "%d %s", osd->model_channellist.channel[i].lcn, osd->model_channellist.channel[i].name); 
      osd_text(osd, x, y , channellist_win_w, row_space_y, color, bg_color, str);  
    }                                        
    y += row_space_y;     
  }
}

/*
 * Display now and next events time and title
 */
static void osd_channellist_now_next_title(struct osd_t* osd, struct event_t *event, int index) 
{
  uint32_t color = COLOR_TEXT;
  uint32_t bg_color = COLOR_BACKGROUND;
  struct tm start_time;
  struct tm stop_time;
  char str[128];
    
  if (event != NULL) {
    localtime_r((time_t*)&event->start, &start_time);
    localtime_r((time_t*)&event->stop, &stop_time);
    snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, event->title);

    if (osd->model_now_next.selectedIndex == index) {
      color = COLOR_SELECTED_TEXT;
      if (osd->model_channellist.active == 0) {
        // now and next window is active
        bg_color = COLOR_SELECTED_BACKGROUND;
      }
    }
    osd_text(osd, nowandnext_win_x + PADDING_X, nowandnext_win_y + PADDING_Y + index * 50, nowandnext_win_w, 50, color, bg_color, str);  
  }
}

/*  
 * Display the channellist view (including channellist, now and next and event info windows)
 */
static void osd_channellist_view(struct osd_t* osd)
{
  if (osd->model_channellist_current.channel[0].id == -1) {
    // not currently displayed, draw everything
    
    // Channellist window
    channellist_win_x = OSD_XMARGIN;
    channellist_win_y = osd->display_height - OSD_YMARGIN - 620;
    channellist_win_w = 500;
    channellist_win_h = 620;
    osd_draw_window(osd, channellist_win_x, channellist_win_y, channellist_win_w, channellist_win_h);

    // Now and Next window
    nowandnext_win_x = channellist_win_x + channellist_win_w + OSD_XMARGIN;
    nowandnext_win_y = osd->display_height - OSD_YMARGIN - 120;
    nowandnext_win_w = osd->display_width - nowandnext_win_x - OSD_XMARGIN;  
    nowandnext_win_h = 120;   
    osd_draw_window(osd, nowandnext_win_x, nowandnext_win_y, nowandnext_win_w, nowandnext_win_h);
    
    // Event info window
    eventinfo_win_x = channellist_win_x + channellist_win_w + OSD_XMARGIN;
    eventinfo_win_y = osd->display_height - OSD_YMARGIN - 120 - 477 - PADDING_Y;
    eventinfo_win_w = osd->display_width - eventinfo_win_x - OSD_XMARGIN;  
    eventinfo_win_h = 477;
    osd_draw_window(osd, eventinfo_win_x, eventinfo_win_y, eventinfo_win_w, eventinfo_win_h);  
  }
  else {
    // clear now and next and event info area
    graphics_resource_fill(osd->img, nowandnext_win_x + 10, nowandnext_win_y + 10, nowandnext_win_w - 20, nowandnext_win_h - 20, COLOR_BACKGROUND);
    graphics_resource_fill(osd->img, eventinfo_win_x + 10, eventinfo_win_y + 10, eventinfo_win_w - 20, eventinfo_win_h - 20, COLOR_BACKGROUND);
  }
    
  // now and next title
  osd_channellist_now_next_title(osd, osd->model_now_next.nowEvent, 0); 
  osd_channellist_now_next_title(osd, osd->model_now_next.nextEvent, 1); 

  // event info
  struct event_t* event;
  if (osd->model_now_next.selectedIndex == 0) {
    event = osd->model_now_next.nowEvent;
  } 
  else {
    event = osd->model_now_next.nextEvent;
  } 
      
  if (event != NULL) {
    osd_text(osd, eventinfo_win_x + PADDING_X, eventinfo_win_y + PADDING_Y, eventinfo_win_w, 50, COLOR_SELECTED_TEXT, COLOR_BACKGROUND, event->title);
    osd_paragraph(osd, event->description, 40, eventinfo_win_x + PADDING_X, eventinfo_win_y + PADDING_Y + 50, eventinfo_win_w - 2 * PADDING_X, eventinfo_win_h - 2 * PADDING_Y - 50);
  }

  osd_channellist_channels(osd);
}

/*  
 * Display the menu view 
 */
static void osd_menu_view(struct osd_t* osd)
{
  osd_draw_window(osd, OSD_XMARGIN, OSD_YMARGIN, osd->display_width - 2 * OSD_XMARGIN, 300);
  osd_text(osd, OSD_XMARGIN + PADDING_X, OSD_YMARGIN + PADDING_Y, osd->display_width - 2 * OSD_XMARGIN, 300, COLOR_TEXT, COLOR_BACKGROUND, osd->model_menu.info);
  osd_text(osd, OSD_XMARGIN + PADDING_X, OSD_YMARGIN + PADDING_Y + 50, osd->display_width - 2 * OSD_XMARGIN, 300, COLOR_TEXT, COLOR_BACKGROUND, osd->model_menu.bitrate);  
}

/*
 * view dispatcher
 */
void osd_view(struct osd_t* osd, int view)
{
  pthread_mutex_lock(&osd->osd_mutex);
  
  switch (view) {
    case OSD_CHANNELLIST:
      osd_channellist_view(osd);
      break;
    case OSD_MENU:
      osd_menu_view(osd);
      break;
  }
  osd->osd_state = view;
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);  
  pthread_mutex_unlock(&osd->osd_mutex);
}
