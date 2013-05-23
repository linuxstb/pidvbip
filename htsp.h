#ifndef _HTSP_H
#define _HTSP_H

#include <stdint.h>
#include <pthread.h>

#define MAX_HTSP_SERVERS 2

struct htsp_t 
{
    int numservers;
    struct sockaddr_in *remote[MAX_HTSP_SERVERS];
    int sock[MAX_HTSP_SERVERS];
    char* ip[MAX_HTSP_SERVERS];
    char* host[MAX_HTSP_SERVERS];
    int port[MAX_HTSP_SERVERS];
    unsigned char challange[MAX_HTSP_SERVERS][32];
    int subscriptionId;
    int subscriptionServer;
    pthread_mutex_t htsp_mutex;
};

struct htsp_message_t
{
    int server;
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
  int width;
  int height;
  char lang[4];
  int audio_type;  /* iso639_language_descriptor audio_type */
};

struct htsp_subscription_t
{
  struct htsp_stream_t* streams;
  int numstreams;
  int numaudiostreams;
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
int htsp_connect(struct htsp_t* htsp, int server);
int htsp_create_message(struct htsp_message_t* msg, ...);
int htsp_send_message(struct htsp_t* htsp, int server, struct htsp_message_t* msg);
int htsp_recv_message(struct htsp_t* htsp, int server, struct htsp_message_t* msg, int timeout);
int htsp_login(struct htsp_t* htsp, int server, char* tvh_user, char* tvh_pass);
char* htsp_get_string(struct htsp_message_t* msg, char* name);
int htsp_get_int(struct htsp_message_t* msg, char* name, int32_t* val);
int htsp_get_uint(struct htsp_message_t* msg, char* name, uint32_t* val);
int htsp_get_int64(struct htsp_message_t* msg, char* name, int64_t* val);
int htsp_get_bin(struct htsp_message_t* msg, char* name, unsigned char** data,int* size);
int htsp_get_list(struct htsp_message_t* msg, char* name, unsigned char** data,int* size);

int htsp_parse_subscriptionStart(struct htsp_message_t* msg, struct htsp_subscription_t*);
int htsp_send_skip(struct htsp_t* htsp, int server, int time);

#endif
