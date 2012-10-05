#ifndef _VCODEC_OMX_H
#define _VCODEC_OMX_H

#include "codec.h"
#include "libs/ilclient/ilclient.h"

void vcodec_omx_init(struct codec_t* codec, OMX_IMAGE_CODINGTYPE codectype);
void vcodec_omx_add_to_queue(struct codec_t* codec, struct packet_t* packet);
int64_t vcodec_omx_current_get_pts(struct codec_t* codec);

#endif
