#ifndef _CHANNELS_H
#define _CHANNELS_H

#include <stdint.h>

#define CTYPE_NONE    0
#define CTYPE_UNKNOWN 1
#define CTYPE_SDTV    2
#define CTYPE_HDTV    3
#define CTYPE_RADIO   4

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


void channels_init(void);
void channels_add(int lcn, int id, char* name, int type, uint32_t eventId, uint32_t nextEventId);
void channels_update(int lcn, int id, char* name, int type, uint32_t eventId, uint32_t nextEventId);
struct channel_t* channels_return_struct(void);
void channels_dump(void);
int channels_getid(int lcn);
char* channels_getname(int id);
uint32_t channels_geteventid(int id);
uint32_t channels_getnexteventid(int id);
int channels_getlcn(int id);
int channels_gettype(int id);
int channels_getnext(int id);
int channels_getprev(int id);
int channels_getfirst(void);
int channels_getcount(void);

#endif
