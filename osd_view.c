#include "osd_view.h"

static uint32_t channellist_window_x; 
static uint32_t channellist_window_y;
static uint32_t channellist_window_w;
static uint32_t channellist_window_h;

#if 0
void osd_channellist_show_info(struct osd_t* osd, int channel_id)
{
  char str[128];
  int server;
  
  channels_geteventid(channel_id,&osd->event,&server);
  channels_getnexteventid(channel_id,&osd->nextEvent,&server);

  struct event_t* event = event_copy(osd->event,server);
  struct event_t* nextEvent = event_copy(osd->nextEvent,server);
  //event_dump(event);
  //event_dump(nextEvent);
  snprintf(str,sizeof(str),"%03d - %s",channels_getlcn(channel_id),channels_getname(channel_id));
  char* iso_text = malloc(strlen(str)+1);
  utf8decode(str,iso_text);

  osd_show_time(osd);
  osd_show_eventinfo(osd,event,nextEvent);

  free(iso_text);
  event_free(event);
  event_free(nextEvent);
}

void osd_channellist_show_eventinfo(struct osd_t* osd, int channelId)
{
  int server;
  char str[128];
  struct tm start_time;
  struct tm stop_time;
  char* iso_text;
  uint32_t x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
  uint32_t y = osd->display_height - OSD_YMARGIN - 120 - OSD_YMARGIN - 290;
  uint32_t w = osd->display_width - x - OSD_XMARGIN;
  uint32_t h = 290;
  
  channels_geteventid(channelId, &osd->event, &server);
  channels_getnexteventid(channelId, &osd->nextEvent, &server);

  struct event_t* event = event_copy(osd->event, server);
  struct event_t* nextEvent = event_copy(osd->nextEvent, server);

  snprintf(str, sizeof(str),"%s", "Hejsan skriver lite text har! \n Byter rad och testar!");  
  osd_draw_window(osd, x, y, w, h);
  graphics_resource_render_text_ext(osd->img, x + 10, y + 120, w, 120,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     str, strlen(str), 40); 
}

void osd_channellist_show_events(struct osd_t* osd, int channelId)
{
  int server;
  char str[128];
  struct tm start_time;
  struct tm stop_time;
  char* iso_text;
  uint32_t x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
  uint32_t y = osd->display_height - OSD_YMARGIN - 120;
  uint32_t w = osd->display_width - x - OSD_XMARGIN;
  uint32_t h = 120;
  
  channels_geteventid(channelId, &osd->event, &server);
  channels_getnexteventid(channelId, &osd->nextEvent, &server);

  struct event_t* event = event_copy(osd->event, server);
  struct event_t* nextEvent = event_copy(osd->nextEvent, server);

  osd_draw_window(osd, x, y, w, h);

  if (event == NULL) {
    return;
  }
  
  /* Start/stop time - current event */
  localtime_r((time_t*)&event->start, &start_time);
  localtime_r((time_t*)&event->stop, &stop_time);
  if (event->title) {
    iso_text = malloc(strlen(event->title)+1);
    utf8decode(event->title, iso_text);
  }
  else {
    iso_text = malloc(1);
    iso_text = "";
  }
  
  snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, iso_text);
                                     
  graphics_resource_render_text_ext(osd->img, x + 10, y + 20, w, 50,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     str, strlen(str), 40);                                     
  free(iso_text);
  
  if (nextEvent == NULL)
    return;

  /* Start/stop time - next event */
  localtime_r((time_t*)&nextEvent->start, &start_time);
  localtime_r((time_t*)&nextEvent->stop, &stop_time);
  if (nextEvent->title) {
    iso_text = malloc(strlen(nextEvent->title)+1);
    utf8decode(nextEvent->title, iso_text);
  }
  
  snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, iso_text);
  graphics_resource_render_text_ext(osd->img, x + 10, y + 70, w, 50,
                                     GRAPHICS_RGBA32(0xff,0xff,0xff,0xff), /* fg */
                                     GRAPHICS_RGBA32(0,0,0,0x80), /* bg */
                                     str, strlen(str), 40);
  free(iso_text);
  osd_channellist_show_eventinfo(osd, channelId);
  event_free(event);
  event_free(nextEvent);
}
#endif

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
  
  for (i = 0; i < osd->model_channellist_new.numUsed; i++) {
    if ( compareIndexModelChannelList(&osd->model_channellist_new, &osd->model_channellist, i) == 1 ) {
      printf("osd_channellist_channels: Update index %d - lcn %d\n", i, osd->model_channellist_new.channel[i].lcn);
      if (osd->model_channellist_new.selectedIndex == i) {
        color = COLOR_SELECTED_TEXT;
        bg_color = COLOR_SELECTED_BACKGROUND;
      }
      else {
        color = COLOR_TEXT;
        bg_color = COLOR_BACKGROUND;
      }
      snprintf(str, sizeof(str), "%d %s", osd->model_channellist_new.channel[i].lcn, osd->model_channellist_new.channel[i].name); 
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
  int x, y, w, h;
  uint32_t color = COLOR_TEXT;
  uint32_t bg_color = COLOR_BACKGROUND;
  char str[128];
  struct tm start_time;
  struct tm stop_time;
  
  if (osd->model_channellist.channel[0].id == -1) {
    // not currently displayed, draw everything
    
    // Channellist window
    channellist_window_x = OSD_XMARGIN;
    channellist_window_y = osd->display_height - OSD_YMARGIN - 620;
    channellist_window_w = 500;
    channellist_window_h = 620;
    osd_draw_window(osd, channellist_window_x, channellist_window_y, channellist_window_w, channellist_window_h);

    // Now and Next window
    x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
    y = osd->display_height - OSD_YMARGIN - 120;
    w = osd->display_width - x - OSD_XMARGIN;  
    h = 120;
    osd_draw_window(osd, x, y, w, h);
  }
    
  if (osd->model_now_next_new.nowEvent != NULL) {
    localtime_r((time_t*)&osd->model_now_next_new.nowEvent->start, &start_time);
    localtime_r((time_t*)&osd->model_now_next_new.nowEvent->stop, &stop_time);
    snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, osd->model_now_next_new.nowEvent->title);
    osd_text(osd, x + PADDING_X, y + PADDING_Y, w, 50, COLOR_SELECTED_TEXT, bg_color, str);  
  }
  if (osd->model_now_next_new.nextEvent != NULL) {
    localtime_r((time_t*)&osd->model_now_next_new.nextEvent->start, &start_time);
    localtime_r((time_t*)&osd->model_now_next_new.nextEvent->stop, &stop_time);    
    snprintf(str, sizeof(str),"%02d:%02d - %02d:%02d %s",start_time.tm_hour,start_time.tm_min,stop_time.tm_hour,stop_time.tm_min, osd->model_now_next_new.nextEvent->title);
    osd_text(osd, x + PADDING_X, y + PADDING_Y + 50, w, 50, color, bg_color, str);
  }
  
  // Event info window
  x = channellist_window_x + channellist_window_w + OSD_XMARGIN;
  y = osd->display_height - OSD_YMARGIN - 120 - 220 - PADDING_Y;
  w = osd->display_width - x - OSD_XMARGIN;  
  h = 220;
  osd_draw_window(osd, x, y, w, h);    

  if (osd->model_now_next_new.nowEvent != NULL) {
    osd_text(osd, x + PADDING_X, y + PADDING_Y, w, 50, color, bg_color, osd->model_now_next_new.nextEvent->description);    
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
