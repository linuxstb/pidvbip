#include "osd_view.h"

static uint32_t channellist_window_x; 
static uint32_t channellist_window_y;
static uint32_t channellist_window_w;
static uint32_t channellist_window_h;

static uint32_t nowandnext_window_x;
static uint32_t nowandnext_window_y;
static uint32_t nowandnext_window_w;
static uint32_t nowandnext_window_h;

static uint32_t eventinfo_window_x;
static uint32_t eventinfo_window_y;
static uint32_t eventinfo_window_w;
static uint32_t eventinfo_window_h;

/* 
 * Display the channels in the channellist window
 */
static void osd_channellist_channels(struct osd_t* osd)
{
  int i;
  char str[60];
  char* iso_text = NULL;
  uint32_t row_space_y = 50;
  uint32_t x = channellist_window_x + PADDING_X;
  uint32_t y = channellist_window_y + PADDING_Y;
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
      iso_text = malloc(strlen(str) + 1);
      utf8decode(str, iso_text);        
      graphics_resource_render_text_ext(osd->img, x, y, channellist_window_w, row_space_y,
                                        color,         /* fg */
                                        bg_color,      /* bg */
                                        iso_text, strlen(iso_text), 40);
                                        
      free(iso_text);                                          
    }                                        
    y += row_space_y;     
  }
}

/*  
 * Display the channellist view (including channellist, Now and Next and Event Info windows)
 */
static void osd_channellist_view(struct osd_t* osd)
{
  uint32_t color = COLOR_TEXT;
  uint32_t bg_color = COLOR_BACKGROUND;
  char str[128];
  struct tm start_time;
  struct tm stop_time;
  
  if (osd->model_channellist_current.channel[0].id == -1) {
    // not currently displayed, draw everything
    
    // Channellist window
    channellist_window_x = OSD_XMARGIN;
    channellist_window_y = osd->display_height - OSD_YMARGIN - 620;
    channellist_window_w = 500;
    channellist_window_h = 620;
    osd_draw_window(osd, channellist_window_x, channellist_window_y, channellist_window_w, channellist_window_h);

    // Now and Next window
    nowandnext_window_x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
    nowandnext_window_y = osd->display_height - OSD_YMARGIN - 120;
    nowandnext_window_w = osd->display_width - nowandnext_window_x - OSD_XMARGIN;  
    nowandnext_window_h = 120;   
    osd_draw_window(osd, nowandnext_window_x, nowandnext_window_y, nowandnext_window_w, nowandnext_window_h);
    
    // Event info window
    eventinfo_window_x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
    eventinfo_window_y = osd->display_height - OSD_YMARGIN - 120 - 477 - PADDING_Y;
    eventinfo_window_w = osd->display_width - eventinfo_window_x - OSD_XMARGIN;  
    eventinfo_window_h = 477;
    osd_draw_window(osd, eventinfo_window_x, eventinfo_window_y, eventinfo_window_w, eventinfo_window_h);  
  }
  else {
    // clear now and next and event info area
    graphics_resource_fill(osd->img, nowandnext_window_x + 10, nowandnext_window_y + 10, nowandnext_window_w - 20, nowandnext_window_h - 20, COLOR_BACKGROUND);
    graphics_resource_fill(osd->img, eventinfo_window_x + 10, eventinfo_window_y + 10, eventinfo_window_w - 20, eventinfo_window_h - 20, COLOR_BACKGROUND);
  }
  
  // now
  if (osd->model_now_next.nowEvent != NULL) {
    localtime_r((time_t*)&osd->model_now_next.nowEvent->start, &start_time);
    localtime_r((time_t*)&osd->model_now_next.nowEvent->stop, &stop_time);
    snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, osd->model_now_next.nowEvent->title);
    bg_color = COLOR_BACKGROUND;
    if (osd->model_now_next.selectedIndex == 0) {
      color = COLOR_SELECTED_TEXT;
      if (osd->model_channellist.active == 0) {
        bg_color = COLOR_SELECTED_BACKGROUND;
      }
    }
    else {
      color = COLOR_TEXT;
    }
    osd_text(osd, nowandnext_window_x + PADDING_X, nowandnext_window_y + PADDING_Y, nowandnext_window_w, 50, color, bg_color, str);  
  }
  
  // next
  if (osd->model_now_next.nextEvent != NULL) {
    localtime_r((time_t*)&osd->model_now_next.nextEvent->start, &start_time);
    localtime_r((time_t*)&osd->model_now_next.nextEvent->stop, &stop_time);    
    snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, osd->model_now_next.nextEvent->title);
    bg_color = COLOR_BACKGROUND;
    if (osd->model_now_next.selectedIndex == 1) {
      color = COLOR_SELECTED_TEXT;
      if (osd->model_channellist.active == 0) {
        bg_color = COLOR_SELECTED_BACKGROUND;
      }
    }
    else {
      color = COLOR_TEXT;
    }
    osd_text(osd, nowandnext_window_x + PADDING_X, nowandnext_window_y + PADDING_Y + 50, nowandnext_window_w, 50, color, bg_color, str);    
  }
    
  // event info
  if (osd->model_now_next.selectedIndex == 0 && osd->model_now_next.nowEvent != NULL) {
    osd_text(osd, eventinfo_window_x + PADDING_X, eventinfo_window_y + PADDING_Y, eventinfo_window_w, 50, COLOR_SELECTED_TEXT, COLOR_BACKGROUND, osd->model_now_next.nowEvent->title);
    osd_paragraph(osd, osd->model_now_next.nowEvent->description, 40, eventinfo_window_x + PADDING_X, eventinfo_window_y + PADDING_Y + 50, eventinfo_window_w - 2 * PADDING_X, eventinfo_window_h - 2 * PADDING_Y - 50);
  }
  if (osd->model_now_next.selectedIndex == 1 && osd->model_now_next.nextEvent != NULL) {
    osd_text(osd, eventinfo_window_x + PADDING_X, eventinfo_window_y + PADDING_Y, eventinfo_window_w, 50, COLOR_SELECTED_TEXT, COLOR_BACKGROUND, osd->model_now_next.nextEvent->title);
    osd_paragraph(osd, osd->model_now_next.nextEvent->description, 40, eventinfo_window_x + PADDING_X, eventinfo_window_y + PADDING_Y + 50, eventinfo_window_w - 2 * PADDING_X, eventinfo_window_h - 2 * PADDING_Y - 50);
  }

  osd_channellist_channels(osd);
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
  }
  osd->osd_state = view;
  graphics_update_displayed_resource(osd->img, 0, 0, 0, 0);  
  pthread_mutex_unlock(&osd->osd_mutex);
}
