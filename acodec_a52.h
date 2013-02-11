#ifndef _ACODEC_A52_H
#define _ACODEC_A52_H

#include "codec.h"
#include "omx_utils.h"

void acodec_a52_init(struct codec_t* codec, struct omx_pipeline_t* pipe);
void acodec_a52_add_to_queue(struct codec_t* codec, struct packet_t* packet);

#endif
