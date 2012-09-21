#ifndef _EVENTS_H
#define _EVENTS_H

#include <stdint.h>
#include "avl.h"

struct event_t {
  struct avl avl;
  uint32_t eventId;
  uint32_t channelId;
  int64_t start;
  int64_t stop;
  char* title;
  char* description;
  uint32_t serieslinkId;
  uint32_t episodeId;
  uint32_t nextEventId;  
};

void process_event_message(char* method, struct htsp_message_t* msg);
struct event_t* get_event(uint32_t eventId);
void event_delete(uint32_t eventId);
void event_dump(struct event_t* event);

#endif
