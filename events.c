#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "avl.h"
#include "htsp.h"
#include "events.h"

static int cmp_event(void* a, void* b)
{
  int aa, bb;
  aa = ((struct event_t*)a)->eventId;
  bb = ((struct event_t*)b)->eventId;
  return aa - bb;
}

static void event_free_items(struct event_t* event)
{
  if (event->title)
    free(event->title);

  if (event->description)
    free(event->description);
}

static struct avl_tree events = { NULL, cmp_event};

void process_event_message(char* method, struct htsp_message_t* msg)
{
  struct event_t* event;
  uint32_t eventId;
  int do_insert = 0;

  htsp_get_uint(msg,"eventId",&eventId);

  if (strcmp(method,"eventAdd")==0) {
    do_insert = 1;
    event = calloc(sizeof(struct event_t),1);
    event->eventId = eventId;
  } else { // eventUpdate
    event = get_event(eventId);
    event_free_items(event);
    memset(event,0,sizeof(event));
  }

  htsp_get_uint(msg,"eventId",&event->eventId);
  htsp_get_uint(msg,"channelId",&event->channelId);
  htsp_get_int64(msg,"start",&event->start);
  htsp_get_int64(msg,"stop",&event->stop);
  event->title = htsp_get_string(msg,"title");
  event->description = htsp_get_string(msg,"description");
  htsp_get_uint(msg,"serieslinkId",&event->serieslinkId);
  htsp_get_uint(msg,"episodeId",&event->episodeId);
  htsp_get_uint(msg,"nextEventId",&event->nextEventId);

  //htsp_dump_message(msg);

  if (do_insert) {
    avl_insert(&events,(struct avl*)event);
  }
}

static struct event_t* searched_event;

static int iter(struct avl* a)
{
  searched_event = (struct event_t*)a;
  fprintf(stderr,"searched_event->eventId=%d\n",searched_event->eventId);

  return 0;
}

struct event_t* get_event(uint32_t eventId)
{
  struct event_t event;
  event.eventId = eventId;

  fprintf(stderr,"Searching for %d\n",eventId);
  searched_event = NULL;
  avl_range(&events,(struct avl*)&event,(struct avl*)&event,iter);
  return searched_event;
}

void event_delete(uint32_t eventId)
{
  struct event_t* event = get_event(eventId);

  //fprintf(stderr,"DELETING EVENT:\n");
  //event_dump(event);

  if (event) {
    avl_remove(&events,(struct avl*)event);
    event_free_items(event);
    free(event);
  }
}

void event_dump(struct event_t* event)
{
  if (event==NULL) {
    fprintf(stderr,"NULL event\n");
    return;
  }

  struct tm start_time;
  struct tm stop_time;

  int duration = event->stop - event->start;
  localtime_r((time_t*)&event->start,&start_time);
  localtime_r((time_t*)&event->stop,&stop_time);

  fprintf(stderr,"Title:       %s\n",event->title);
  fprintf(stderr,"Start:       %04d-%02d-%02d %02d:%02d:%02d\n",start_time.tm_year+1900,start_time.tm_mon+1,start_time.tm_mday,start_time.tm_hour,start_time.tm_min,start_time.tm_sec);
  fprintf(stderr,"Stop:        %04d-%02d-%02d %02d:%02d:%02d\n",stop_time.tm_year+1900,stop_time.tm_mon+1,stop_time.tm_mday,stop_time.tm_hour,stop_time.tm_min,stop_time.tm_sec);
  fprintf(stderr,"Duration:    %02d:%02d:%02d\n",duration/3600,(duration%3600)/60,duration % 60);
  fprintf(stderr,"Description: %s\n",event->description);
}
