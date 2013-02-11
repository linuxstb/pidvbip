#ifndef _ACODEC_MPEG_H
#define _ACODEC_MPEG_H

#include "codec.h"
#include "omx_utils.h"

void acodec_mpeg_init(struct codec_t* codec, struct omx_pipeline_t* pipe);
void acodec_mpeg_add_to_queue(struct codec_t* codec, struct packet_t* packet);

#endif
