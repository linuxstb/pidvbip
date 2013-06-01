#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "channels.h"
#include "events.h"

/* Channels are merged from different servers, based on LCN */

struct channel_t
{
  int id;   /* Our internal ID */
  int tvh_id[MAX_HTSP_SERVERS]; /* tvheadend ID */
  uint32_t eventId[MAX_HTSP_SERVERS];
  uint32_t nextEventId[MAX_HTSP_SERVERS];
  int lcn;
  int type;
  char* name;
  struct channel_t* next;
  struct channel_t* prev;
};

static struct channel_t* channels;
static struct channel_t* channels_cache;

static int next_id = 0;

void channels_init(void)
{
  channels = NULL;
  channels_cache = NULL;
}

void channels_add(int server, int lcn, int tvh_id, char* name, int type, uint32_t eventId, uint32_t nextEventId)
{
  struct channel_t* p = channels;
  struct channel_t* prev = NULL;

  struct channel_t* new = calloc(sizeof(struct channel_t),1);

  new->lcn = lcn;
  new->tvh_id[server] = tvh_id;
  new->type = type;
  new->name = name;
  new->eventId[server] = eventId;
  new->nextEventId[server] = nextEventId;

  if (channels == NULL) {
    channels = new;
    channels->id = next_id++;
    channels->next = NULL;
    channels->prev = NULL;
  } else {
    while ((p != NULL) && (p->lcn < lcn)) {
      prev = p;
      p = p-> next;
    }

    if (p==NULL) {
      /* Append to list */
      prev->next = new;
      new->id = next_id++;
      new->prev = prev;
      new->next = NULL;
    } else {
      if (p->lcn == lcn) {
        /* Update an existing channel */
        p->tvh_id[server] = tvh_id;
        p->eventId[server] = eventId;
        p->nextEventId[server] = nextEventId;
      } else if (p->prev == NULL) {
        /* Add to start */
        new->id = next_id++;
        new->next = channels;
        new->prev = NULL;
        channels->prev = new;
        channels = new;
      } else {
        /* Add in middle */
        new->id = next_id++;
        new->next = p;
        new->prev = prev;
        prev->next = new;
        p->prev = new;
      }
    }
  }
}

void channels_update(int server, int lcn, int tvh_id, char* name, int type, uint32_t eventId, uint32_t nextEventId)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->tvh_id[server] == tvh_id)) {
    p = channels_cache;
  } else {
    p = channels;
    while ((p) && (p->tvh_id[server] != tvh_id)) {
      p = p->next;
    }
  }

  if (p==NULL) {
    fprintf(stderr,"Channel %d not found for update, adding.\n",tvh_id);
    channels_add(server,lcn,tvh_id,name,type,eventId,nextEventId);
  } else {
    if (lcn >= 0) p->lcn = lcn;
    if (type) p->type = type;
    if (name) {
      free(p->name);
      p->name = name;
    }
    if (eventId) p->eventId[server] = eventId;
    if (nextEventId) p->nextEventId[server] = nextEventId;
  }
}

void channels_dump(void)
{
  struct channel_t* p = channels;
  int i;

  fprintf(stderr,"id     tvh_id  lcn   name\n");
  while (p) {
    for (i=0;i<MAX_HTSP_SERVERS;i++) {
      if (p->tvh_id[i]) {
        fprintf(stderr,"id=%5d  %5d - %s",p->id,p->lcn,p->name);
        int j = 25 - strlen(p->name);
        while (j--)
          fprintf(stderr," ");

        struct event_t* event = event_copy(p->eventId[i],i);
        if (event) { 
          fprintf(stderr,"%s",event->title);
          event_free(event);
        } else {
          fprintf(stderr,"[no event]");
        }
        fprintf(stderr," [server %d, tvh_id %d]\n",i,p->tvh_id[i]);
      }
    }
    p = p->next;
  }
}

int channels_getid(int lcn)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->lcn == lcn)) {
    return channels_cache->id;
  }

  p = channels;
  while (p) {
    if (p->lcn == lcn) {
      channels_cache = p;
      return p->id;
    }
    p = p->next;
  }

  return -1;
}

char* channels_getname(int id)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache->name;;
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      return p->name;
    }
    p = p->next;
  }

  return "[NO CHANNEL]";
}

void channels_geteventid(int id, uint32_t* eventid, int* server)
{
  struct channel_t* p;
  int i;

  if ((channels_cache) && (channels_cache->id == id)) {
    for (i=0;i<MAX_HTSP_SERVERS;i++) {
      if (channels_cache->tvh_id[i]) {
        *eventid = channels_cache->eventId[i];
        *server = i;
        return;
      }
    }
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      for (i=0;i<MAX_HTSP_SERVERS;i++) {
        if (p->tvh_id[i]) {
          *eventid = p->eventId[i];
          *server = i;
          return;
        }
      }
    }
    p = p->next;
  }

  *eventid = 0;
  *server = -1;
}

void channels_getnexteventid(int id, uint32_t* eventid, int* server)
{
  struct channel_t* p;
  int i;

  if ((channels_cache) && (channels_cache->id == id)) {
    for (i=0;i<MAX_HTSP_SERVERS;i++) {
      if (channels_cache->tvh_id[i]) {
        *eventid = channels_cache->nextEventId[i];
        *server = i;
        return;
      }
    }
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      for (i=0;i<MAX_HTSP_SERVERS;i++) {
        if (p->tvh_id[i]) {
          *eventid = p->nextEventId[i];
          *server = i;
          return;
        }
      }
    }
    p = p->next;
  }

  *eventid = 0;
  *server = -1;
}

void channels_gettvhid(int id, int* tvh_id, int* server)
{
  struct channel_t* p;
  int i;

  if ((channels_cache) && (channels_cache->id == id)) {
    for (i=0;i<MAX_HTSP_SERVERS;i++) {
      if (channels_cache->tvh_id[i]) {
        *tvh_id = channels_cache->tvh_id[i];
        *server = i;
        return;
      }
    }
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      for (i=0;i<MAX_HTSP_SERVERS;i++) {
        if (p->tvh_id[i]) {
          *tvh_id = p->tvh_id[i];
          *server = i;
          return;
        }
      }
    }
    p = p->next;
  }

  *tvh_id = 0;
  *server = -1;
}

int channels_getlcn(int id)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache->lcn;;
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      return p->lcn;
    }
    p = p->next;
  }

  return -1;
}

int channels_gettype(int id)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache->type;;
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      return p->type;
    }
    p = p->next;
  }

  return -1;
}

static struct channel_t* find_channel(int id)
{
  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache;
  } else {
    struct channel_t* p = channels;

    while (p && p->id != id)
      p = p->next;

    return p;
  }
}

int channels_getnext(int id)
{
  struct channel_t* p = find_channel(id);

  if (p==NULL) {
    return channels_getfirst();
  } else {
    if (p->next == NULL) {
      channels_cache = channels;
    } else {
      channels_cache = p->next;
    }
    return channels_cache->id;
  }
}

int channels_getprev(int id)
{
  struct channel_t* p = find_channel(id);

  if (p==NULL) {
    return channels_getfirst();
  } else {
    channels_cache = p->prev;
    if (p->prev == NULL) {
      p = channels;
      while (p->next != NULL) {
        p = p->next;
      }
      channels_cache = p;
    }
    return channels_cache->id;
  }
}

int channels_getfirst(void)
{
  channels_cache = channels;
  return channels->id;
}

int channels_getcount(void)
{
  /* TODO: Doesn't include deleted channels */
  return next_id;
}

