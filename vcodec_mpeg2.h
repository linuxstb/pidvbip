#ifndef _MPEG2_H
#define _MPEG2_H

#include <stdint.h>
#include <pthread.h>
#include "libmpeg2/mpeg2.h"
#include "vo_pi.h"

struct mpeg2_packet_t
{
  unsigned char* buf;     /* The buffer to be freed after use */
  unsigned char* packet;  /* Pointer to the actual video data (within buf) */
  int packetlength;       /* Number of bytes in packet */
  int64_t PTS;
  int64_t DTS;
};

struct decoder_queue_t
{
  struct mpeg2_packet_t* data;
  struct decoder_queue_t* prev;
  struct decoder_queue_t* next;
};

struct vcodec_mpeg2_t
{
  mpeg2dec_t * decoder;
  pthread_t thread;
  pthread_mutex_t queue_mutex;
  pthread_cond_t queue_count_cv;
  struct decoder_queue_t* current;
  struct decoder_queue_t* queue_head;
  struct decoder_queue_t* queue_tail;
  int queue_count;
  double nextframetime;
  RECT_VARS_T vars;
};

void vcodec_mpeg2_init(struct vcodec_mpeg2_t* decoder);
void vcodec_mpeg2_add_to_queue(struct vcodec_mpeg2_t* decoder, struct mpeg2_packet_t* packet);
int64_t vcodec_mpeg2_current_get_pts(struct vcodec_mpeg2_t* decoder);

#endif
