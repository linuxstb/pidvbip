#ifndef _EVENTS_H
#define _EVENTS_H

#include <stdint.h>
#include "htsp.h"
#include "list.h"

struct event_t {
  struct list_head list;
  int server;         /* Which server is this from? */
  uint32_t eventId;
  uint32_t channelId;
  int64_t start;
  int64_t stop;
  char* title;
  char* description;
  uint32_t serieslinkId;
  char* serieslinkUri;
  uint32_t episodeId;
  char* episodeUri;
  uint32_t seasonNumber;
  uint32_t episodeNumber;
  uint32_t nextEventId;  
};

void process_event_message(char* method, struct htsp_message_t* msg);
struct event_t* event_get(uint32_t eventId, int server);
struct event_t* event_copy(uint32_t eventId, int server);
void event_delete(uint32_t eventId, int server);
void event_free(struct event_t* event);
void event_dump(struct event_t* event);
int event_find_hd_version(int eventId, int server);
void events_init(void);

#endif
