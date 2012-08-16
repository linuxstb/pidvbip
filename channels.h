#ifndef _CHANNELS_H
#define _CHANNELS_H

void channels_init(void);
void channels_add(int lcn, int id, char* name);
void channels_dump(void);
int channels_getid(int lcn);
char* channels_getname(int id);
int channels_getlcn(int id);
int channels_getnext(int id);
int channels_getprev(int id);
int channels_getfirst(void);
int channels_getcount(void);

#endif
