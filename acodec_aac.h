#ifndef _ACODEC_AAC_H
#define _ACODEC_AAC_H

#include "omx_utils.h"

void acodec_aac_init(struct codec_t* codec, struct omx_pipeline_t* pipe);
void acodec_aac_add_to_queue(struct codec_t* codec, struct packet_t* packet);

#endif
