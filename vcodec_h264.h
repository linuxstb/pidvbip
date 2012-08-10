#ifndef _VCODEC_H264_H
#define _VCODEC_H264_H

#include "codec.h"

void vcodec_h264_init(struct codec_t* codec);
void vcodec_h264_add_to_queue(struct codec_t* codec, struct packet_t* packet);
int64_t vcodec_h264_current_get_pts(struct codec_t* codec);

#endif
