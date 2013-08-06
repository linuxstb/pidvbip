#ifndef _OSD_MODEL_H
#define _OSD_MODEL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CHANNELLIST_NUM_CHANNELS 12

typedef struct {
  int id;
  char info[30];
  char bitrate[30];
} model_menu_t;

typedef struct {
  int selectedIndex;  
  struct event_t* nowEvent;
  struct event_t* nextEvent;
} model_now_next_t;

typedef struct {
  int id;
  int lcn;
  char name[30];
} model_channels_t;

typedef struct {
  int active;
  int selectedIndex;
  int numUsed;
  model_channels_t channel[CHANNELLIST_NUM_CHANNELS];
} model_channellist_t;

void clearModelChannelList(model_channellist_t*);
void setModelChannelList(model_channellist_t*, int, int id, int, char*, int);
void copyModelChannelList(model_channellist_t*, const model_channellist_t*);
int compareIndexModelChannelList(model_channellist_t*, model_channellist_t*, int);
void setModelNowNext(model_now_next_t *model, uint32_t nowEvent, uint32_t nextEvent, int server);
void clearModelNowNext(model_now_next_t *model);

#endif

