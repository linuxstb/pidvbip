#ifndef _CHANNELS_H
#define _CHANNELS_H

#include <stdint.h>
#include "events.h"

#define CTYPE_NONE    0
#define CTYPE_UNKNOWN 1
#define CTYPE_SDTV    2
#define CTYPE_HDTV    3
#define CTYPE_RADIO   4

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

void channels_init(void);
void channels_add(int server, int lcn, int tvh_id, char* name, int type, uint32_t eventId, uint32_t nextEventId);
void channels_update(int server, int lcn, int tvh_id, char* name, int type, uint32_t eventId, uint32_t nextEventId);
struct channel_t* channels_return_struct(void);
void channels_dump(void);
int channels_getid(int lcn);
char* channels_getname(int id);
void channels_geteventid(int id, uint32_t* eventid, int* server);
void channels_getnexteventid(int id, uint32_t* eventid, int* server);
void channels_gettvhid(int id, int* tvh_id, int* server);
int channels_getlcn(int id);
int channels_gettype(int id);
int channels_getnext(int id);
int channels_getprev(int id);
int channels_getfirst(void);
int channels_getcount(void);

#endif
