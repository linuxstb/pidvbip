#ifndef _VCODEC_OMX_H
#define _VCODEC_OMX_H

#include "codec.h"
#include "omx_utils.h"

void vcodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pipe);
void vcodec_omx_add_to_queue(struct codec_t* codec, struct packet_t* packet);
int64_t vcodec_omx_current_get_pts(struct codec_t* codec);

#endif
