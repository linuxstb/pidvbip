#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "htsp.h"
#include "events.h"
#include "channels.h"
#include "list.h"

//#define DEBUG_EVENTS

/* Events are stored in an array of 512*1024 elements, using the lower
   19 bits of the eventId as the index to the array.  The array item
   is a list of events 
*/

#define EVENT_HASH_MASK  0x7ffff
static struct list_head* events[EVENT_HASH_MASK+1];  /* 512*1024 */

static pthread_mutex_t events_mutex;

static struct event_t* event_get_nolock(uint32_t eventId, int server)
{
  struct list_head* head = events[(eventId * MAX_HTSP_SERVERS + server) & EVENT_HASH_MASK];

  if (head) {
    struct list_head* tmp;
    list_for_each(tmp, head) {
      struct event_t* event = list_entry(tmp, struct event_t, list);
      if ((event->eventId == eventId) && (event->server == server))
        return event;
    }
  }
  return NULL;
}

struct event_t* event_get(uint32_t eventId, int server)
{
  pthread_mutex_lock(&events_mutex);
  struct event_t* event = event_get_nolock(eventId, server);
  pthread_mutex_unlock(&events_mutex);

  return event;
}

static void event_free_items(struct event_t* event)
{
  if (event->title)
    free(event->title);

  if (event->description)
    free(event->description);

  if (event->serieslinkUri)
    free(event->serieslinkUri);

  if (event->episodeUri)
    free(event->episodeUri);
}

void event_free(struct event_t* event)
{
  if (!event)
    return;

  event_free_items(event);

  free(event);
}


void process_event_message(char* method, struct htsp_message_t* msg)
{
  struct event_t* event;
  uint32_t eventId;
  int do_insert = 0;

  htsp_get_uint(msg,"eventId",&eventId);

  pthread_mutex_lock(&events_mutex);
  event = event_get_nolock(eventId, msg->server);

#ifdef DEBUG_EVENTS
  if (strcmp(method,"eventUpdate")==0) {
    fprintf(stderr,"eventUpdate - %d\n",eventId);
  } else {
    fprintf(stderr,"eventAdd - %d\n",eventId);
  }
#endif

  if (event == NULL) {
    do_insert = 1;
    event = calloc(sizeof(struct event_t),1);
    event->eventId = eventId;
    if (strcmp(method,"eventUpdate")==0) {
      fprintf(stderr,"WARNING: eventUpdate received for non-existent event %d, adding instead.\n",eventId);
    }
  } else {
    event_free_items(event);
    memset(event,0,sizeof(event));
    if (strcmp(method,"eventAdd")==0) {
      fprintf(stderr,"WARNING: eventAdd received for existing event %d, updating instead.\n",eventId);
    }
  }

  htsp_get_uint(msg,"eventId",&event->eventId);
  htsp_get_uint(msg,"channelId",&event->channelId);
  htsp_get_int64(msg,"start",&event->start);
  htsp_get_int64(msg,"stop",&event->stop);
  event->title = htsp_get_string(msg,"title");
  event->description = htsp_get_string(msg,"description");
  htsp_get_uint(msg,"serieslinkId",&event->serieslinkId);
  event->serieslinkUri = htsp_get_string(msg,"serieslinkUri");
  htsp_get_uint(msg,"episodeId",&event->episodeId);
  htsp_get_uint(msg,"episodeNumber",&event->episodeNumber);
  htsp_get_uint(msg,"seasonNumber",&event->seasonNumber);
  event->episodeUri = htsp_get_string(msg,"episodeUri");
  htsp_get_uint(msg,"nextEventId",&event->nextEventId);

  //htsp_dump_message(msg);

  event->server = msg->server;

  eventId = eventId * MAX_HTSP_SERVERS + msg->server;

  if (do_insert) {
    if (!events[eventId & EVENT_HASH_MASK]) {
      /* Create the list */
      events[eventId & EVENT_HASH_MASK] = malloc(sizeof(struct list_head));
      INIT_LIST_HEAD(events[eventId & EVENT_HASH_MASK]);
    }
    if (!list_empty(events[eventId & EVENT_HASH_MASK])) {
      fprintf(stderr,"INFO: Event hash clash\n");
    }
    list_add(&event->list, events[eventId & EVENT_HASH_MASK]);

#ifdef DEBUG_EVENTS
    struct event_t* event2 = event_get_nolock(eventId);
    if (event2 == NULL) {
      fprintf(stderr,"ERROR: Inserted event %d but could not retrieve it.\n",eventId);
    }
#endif
  }
  pthread_mutex_unlock(&events_mutex);
}

void event_delete(uint32_t eventId, int server)
{
  pthread_mutex_lock(&events_mutex);
  struct event_t* event = event_get_nolock(eventId, server);
  if (event) {
    list_del(&event->list);
    event_free(event);
  }
  pthread_mutex_unlock(&events_mutex);
}

struct event_t* event_copy(uint32_t eventId, int server)
{
  struct event_t* event;
  struct event_t* copy;

  pthread_mutex_lock(&events_mutex);
  event = event_get_nolock(eventId,server);

  if (event==NULL) {
    pthread_mutex_unlock(&events_mutex);
    return NULL;
  }

  copy = malloc(sizeof(struct event_t));

  *copy = *event;
  if (event->title)
    copy->title = strdup(event->title);

  if (event->description)
    copy->description = strdup(event->description);

  if (event->episodeUri)
    copy->episodeUri = strdup(event->episodeUri);

  if (event->serieslinkUri)
    copy->serieslinkUri = strdup(event->serieslinkUri);

  pthread_mutex_unlock(&events_mutex);

  return copy;
}

void event_dump(struct event_t* event)
{
  pthread_mutex_lock(&events_mutex);

  if (event==NULL) {
    fprintf(stderr,"NULL event\n");
    pthread_mutex_unlock(&events_mutex);
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
  fprintf(stderr,"Season:      %d\n",event->seasonNumber);
  fprintf(stderr,"Episode:     %d\n",event->episodeNumber);
  fprintf(stderr,"Description: %s\n",event->description);
  fprintf(stderr,"Episode ID:  %d\n",event->episodeId);
  if (event->episodeUri) fprintf(stderr,"EpisodeUri:  %s\n",event->episodeUri);
  if (event->serieslinkUri) fprintf(stderr,"SerieslinkUri:  %s\n",event->serieslinkUri);

  pthread_mutex_unlock(&events_mutex);
}

int event_find_hd_version(int eventId, int server)
{
  pthread_mutex_lock(&events_mutex);

  struct event_t* current_event = event_get_nolock(eventId,server);

  fprintf(stderr,"Searching for episode %d\n",current_event->episodeId);
  int i;
  int res = -1;
  for (i=0;i<=EVENT_HASH_MASK && res==-1;i++) {
    if (events[i]) {
      struct list_head* tmp;
      list_for_each(tmp, events[i]) {
        struct event_t* event = list_entry(tmp, struct event_t, list);
        if (((event->eventId != eventId) || (event->server != server)) &&
            (event->episodeId == current_event->episodeId) && 
            (event->start == current_event->start) && 
            (channels_gettype(event->channelId)==CTYPE_HDTV)) {   /* FIXME: This is broken since the change to multiple servers */
          res = event->channelId;
        }
      }
    }
  }
  fprintf(stderr,"HERE - res=%d\n",res);
  
  pthread_mutex_unlock(&events_mutex);

  return res;
}

void events_init(void)
{
  memset(events,0,sizeof(events));
  pthread_mutex_init(&events_mutex,NULL);
}
