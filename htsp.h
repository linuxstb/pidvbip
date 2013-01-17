#ifndef _HTSP_H
#define _HTSP_H

#include <stdint.h>
#include <pthread.h>

struct htsp_t 
{
    struct sockaddr_in *remote;
    int sock;
    char* ip;
    char* host;
    int port;
    unsigned char challange[32];
    int subscriptionId;
    pthread_mutex_t htsp_mutex;
};

struct htsp_message_t
{
    unsigned char* msg;
    int msglen;
};

#define HMF_UNKNOWN 0

#define HMF_STREAM_VIDEO 1
#define HMF_STREAM_AUDIO 2
#define HMF_STREAM_SUB 3

#define HMF_VIDEO_CODEC_MPEG2 1
#define HMF_VIDEO_CODEC_H264  2

#define HMF_AUDIO_CODEC_MPEG 1
#define HMF_AUDIO_CODEC_AAC  2
#define HMF_AUDIO_CODEC_AC3  3

#define HMF_SUB_CODEC_DVBSUB  1

struct htsp_stream_t
{
  int index;
  int type;
  int codec;
  char lang[4];
};

struct htsp_subscription_t
{
  struct htsp_stream_t* streams;
  int numstreams;
  int videostream;
  int audiostream;
};


#define HMF_NULL 0
#define HMF_MAP  1
#define HMF_S64  2
#define HMF_STR  3
#define HMF_BIN  4
#define HMF_LIST 5
#define HMF_DBL  6

void htsp_init(struct htsp_t* htsp);
void htsp_lock(struct htsp_t* htsp);
void htsp_unlock(struct htsp_t* htsp);
void htsp_dump_message(struct htsp_message_t* msg);
void htsp_destroy_message(struct htsp_message_t* msg);
int htsp_connect(struct htsp_t* htsp);
int htsp_create_message(struct htsp_message_t* msg, ...);
int htsp_send_message(struct htsp_t* htsp, struct htsp_message_t* msg);
int htsp_recv_message(struct htsp_t* htsp, struct htsp_message_t* msg, int timeout);
int htsp_login(struct htsp_t* htsp);
char* htsp_get_string(struct htsp_message_t* msg, char* name);
int htsp_get_int(struct htsp_message_t* msg, char* name, int32_t* val);
int htsp_get_uint(struct htsp_message_t* msg, char* name, uint32_t* val);
int htsp_get_int64(struct htsp_message_t* msg, char* name, int64_t* val);
int htsp_get_bin(struct htsp_message_t* msg, char* name, unsigned char** data,int* size);
int htsp_get_list(struct htsp_message_t* msg, char* name, unsigned char** data,int* size);

int htsp_parse_subscriptionStart(struct htsp_message_t* msg, struct htsp_subscription_t*);
int htsp_send_skip(struct htsp_t* htsp, int time);

#endif
