#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "channels.h"
#include "events.h"

struct channel_t
{
  int id;
  uint32_t eventId;
  uint32_t nextEventId;
  int lcn;
  int type;
  char* name;
  struct channel_t* next;
  struct channel_t* prev;
};

static struct channel_t* channels;
static struct channel_t* channels_cache;
static int num_channels;

void channels_init(void)
{
  channels = NULL;
  channels_cache = NULL;
  num_channels = 0;
}

void channels_add(int lcn, int id, char* name, int type, uint32_t eventId, uint32_t nextEventId)
{
  struct channel_t* p = channels;
  struct channel_t* prev = NULL;

  struct channel_t* new = malloc(sizeof(struct channel_t));

  new->lcn = lcn;
  new->id = id;
  new->type = type;
  new->name = name;
  new->eventId = eventId;
  new->nextEventId = nextEventId;

  if (channels == NULL) {
    channels = new;
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
      new->prev = prev;
      new->next = NULL;
    } else {
      if (p->prev == NULL) {
        /* Add to start */
        new->next = channels;
        new->prev = NULL;
        channels->prev = new;
        channels = new;
      } else {
        /* Add in middle */
        new->next = p;
        new->prev = prev;
        prev->next = new;
        p->prev = new;
      }
    }
  }

  num_channels++;
}

void channels_update(int lcn, int id, char* name, int type, uint32_t eventId, uint32_t nextEventId)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    p = channels_cache;
  } else {
    p = channels;
    while ((p) && (p->id != id)) {
      p = p->next;
    }
  }

  if (p==NULL) {
    fprintf(stderr,"Channel %d not found for update, adding.\n",id);
    channels_add(lcn,id,name,type,eventId,nextEventId);
  } else {
    if (lcn >= 0) p->lcn = lcn;
    if (type) p->type = type;
    if (name) {
      free(p->name);
      p->name = name;
    }
    if (eventId) p->eventId = eventId;
    if (nextEventId) p->nextEventId = nextEventId;
  }
}

void channels_dump(void)
{
  struct channel_t* p = channels;

  while (p) {
    fprintf(stderr,"%5d  %5d - %s",p->id,p->lcn,p->name);
    int i = 25 - strlen(p->name);
    while (i--)
      fprintf(stderr," ");

    struct event_t* event = event_copy(p->eventId);
    if (event) { 
      fprintf(stderr,"%s\n",event->title);
      event_free(event);
    } else {
      fprintf(stderr,"[no event]\n");
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

uint32_t channels_geteventid(int id)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache->eventId;
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      return p->eventId;
    }
    p = p->next;
  }

  return 0;
}

uint32_t channels_getnexteventid(int id)
{
  struct channel_t* p;

  if ((channels_cache) && (channels_cache->id == id)) {
    return channels_cache->nextEventId;
  }

  p = channels;
  while (p) {
    if (p->id == id) {
      channels_cache = p;
      return p->nextEventId;
    }
    p = p->next;
  }

  return 0;
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
        channels_cache = p;
        p = p->next;
      }
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
  return num_channels;
}

