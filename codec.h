#ifndef _CODEC_H
#define _CODEC_H

#include <stdint.h>
#include <pthread.h>

#include "vo_pi.h"
#include "omx_utils.h"
#include "htsp.h"

struct codec_init_args_t
{
  struct codec_t* codec;
  struct omx_pipeline_t* pipe;
};

struct packet_t
{
  unsigned char* buf;     /* The buffer to be freed after use */
  unsigned char* packet;  /* Pointer to the actual video data (within buf) */
  int packetlength;       /* Number of bytes in packet */
  int64_t PTS;
  int64_t DTS;
};

#define MSG_PACKET       1
#define MSG_STOP         2
#define MSG_PAUSE        3
#define MSG_NEW_CHANNEL  4

struct codec_queue_t
{
  int msgtype;
  struct packet_t* data;
  struct codec_queue_t* prev;
  struct codec_queue_t* next;
};

struct codec_t
{
  void* codecstate;
  int is_running;
  pthread_t thread;
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_count_cv;
  pthread_cond_t resume_cv;
  struct codec_queue_t* queue_head;
  struct codec_queue_t* queue_tail;
  int queue_count;
  int64_t PTS;
  pthread_mutex_t PTS_mutex;
  pthread_mutex_t isrunning_mutex;
  unsigned char* codecdata;
  int codecdatasize;
  struct codec_t* acodec;
  OMX_VIDEO_CODINGTYPE vcodectype;
  int width;
  int height;
  OMX_AUDIO_CODINGTYPE acodectype;
  int first_packet;
};

struct codecs_t {
  struct codec_t vcodec;
  struct codec_t acodec; // Audio
  struct codec_t scodec; // Subtitles
  struct htsp_subscription_t subscription;  // Details of the currently tuned channel
  int is_paused;
};

void codec_queue_init(struct codec_t* codec);
void codec_new_channel(struct codec_t* codec);
void codec_stop(struct codec_t* codec);
void codec_pause(struct codec_t* codec);
void codec_resume(struct codec_t* codec);
void codec_queue_add_item(struct codec_t* codec, struct packet_t* packet);
void codec_queue_free_item(struct codec_t* codec,struct codec_queue_t* item);
struct codec_queue_t* codec_queue_get_next_item(struct codec_t* codec);
void codec_set_pts(struct codec_t* codec, int64_t PTS);
int64_t codec_get_pts(struct codec_t* codec);
int codec_is_running(struct codec_t* codec);
void codec_flush_queue(struct codec_t* codec);

#endif
