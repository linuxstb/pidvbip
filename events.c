#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "avl.h"
#include "htsp.h"
#include "events.h"
#include "channels.h"

static struct avl_tree events;
static pthread_mutex_t events_mutex;

static struct event_t* searched_event;

static int iter(struct avl* a)
{
  searched_event = (struct event_t*)a;
  //fprintf(stderr,"searched_event->eventId=%d\n",searched_event->eventId);

  return 0;
}

static struct event_t* event_get_nolock(uint32_t eventId)
{
  struct event_t event;
  event.eventId = eventId;

  //fprintf(stderr,"Searching for %d\n",eventId);
  searched_event = NULL;
  avl_range(&events,(struct avl*)&event,(struct avl*)&event,iter);
  return searched_event;
}

struct event_t* event_get(uint32_t eventId)
{
  pthread_mutex_lock(&events_mutex);
  struct event_t* event = event_get_nolock(eventId);
  pthread_mutex_unlock(&events_mutex);

  return event;
}


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
  event = event_get_nolock(eventId);

  //  if (strcmp(method,"eventUpdate")==0) {
  //    fprintf(stderr,"eventUpdate - %d\n",eventId);
  //  } else {
  //    fprintf(stderr,"eventAdd - %d\n",eventId);
  //  }

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
  htsp_get_uint(msg,"episodeId",&event->episodeId);
  event->episodeUri = htsp_get_string(msg,"episodeUri");
  htsp_get_uint(msg,"nextEventId",&event->nextEventId);

  //htsp_dump_message(msg);

  if (do_insert) {
    avl_insert(&events,(struct avl*)event);
  }
  pthread_mutex_unlock(&events_mutex);
}

void event_delete(uint32_t eventId)
{
  pthread_mutex_lock(&events_mutex);

  struct event_t* event = event_get_nolock(eventId);

  //fprintf(stderr,"DELETING EVENT:\n");
  //event_dump(event);

  if (event) {
    avl_remove(&events,(struct avl*)event);
    event_free_items(event);
    free(event);
  }
  pthread_mutex_unlock(&events_mutex);
}

struct event_t* event_copy(uint32_t eventId)
{
  struct event_t* event;
  struct event_t* copy;

  pthread_mutex_lock(&events_mutex);
  event = event_get_nolock(eventId);

  if (event==NULL)
    return NULL;

  copy = malloc(sizeof(struct event_t));

  *copy = *event;
  if (event->title)
    copy->title = strdup(event->title);

  if (event->description)
    copy->description = strdup(event->description);

  if (event->episodeUri)
    copy->episodeUri = strdup(event->episodeUri);

  pthread_mutex_unlock(&events_mutex);

  return copy;
}

void event_dump(struct event_t* event)
{
  pthread_mutex_lock(&events_mutex);

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
  if (event->episodeUri) fprintf(stderr,"EpisodeUri:  %s\n",event->episodeUri);

  pthread_mutex_unlock(&events_mutex);
}

static int find_hd_version(struct avl* a,struct event_t* sd_event){
  int res = -1;
  struct event_t* events = (struct event_t*)a;

  if (a==NULL) return -1;

  if ((events->episodeId == sd_event->episodeId) && 
      (events->start == sd_event->start) && 
      (channels_gettype(events->channelId)==CTYPE_HDTV)) {
    return events->channelId;
  }

  if (a->right) {
    res = find_hd_version(a->right,sd_event);
  }

  if (res == -1) {
    if (a->left) {
      res = find_hd_version(a->left,sd_event);
    }
  }

  return res;
}

int event_find_hd_version(int eventId)
{
  pthread_mutex_lock(&events_mutex);

  struct event_t* current_event = event_get_nolock(eventId);

  fprintf(stderr,"Searching for episode %d\n",current_event->episodeId);
  int res = find_hd_version(events.root,current_event);
  fprintf(stderr,"HERE - res=%d\n",res);
  
  pthread_mutex_unlock(&events_mutex);

  return res;
}

void events_init(void)
{
  events.root = NULL;
  events.compar = cmp_event;
  pthread_mutex_init(&events_mutex,NULL);
}
