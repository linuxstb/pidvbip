#ifndef _VCODEC_MPEG2_H
#define _VCODEC_MPEG2_H

#include "codec.h"

void vcodec_mpeg2_init(struct codec_t* codec);
void vcodec_mpeg2_add_to_queue(struct codec_t* codec, struct packet_t* packet);

#endif
