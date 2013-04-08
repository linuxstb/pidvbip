/*

pidvbip - tvheadend client for the Raspberry Pi

(C) Dave Chapman 2012-2013

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "common.h"

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <stdio.h>
#include <fcntl.h>
#include "avplay.h"
#include "codec.h"

#define DUMP_VIDEO

static void convert4(unsigned char* dest, unsigned char* src, int size)
{
  unsigned char* p = src;

  //  memcpy(dest,src,size);
  //  return;

  while (size > 0) {
    int len = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

    dest[0] = 0;
    dest[1] = 0;
    dest[2] = 0;
    dest[3] = 1;
    memcpy(dest+4, p+4, len);
    if (4 + len > size) {
      fprintf(stderr,"Corrupted input, aborting\n");
      exit(1);
    }
    p += 4 + len;
    size -= 4 + len;
    dest += 4 + len;
  }
}

static int map_vcodec(enum CodecID id)
{
  printf("Mapping video codec ID %d (%x)\n", id, id);
  switch (id) {
    case    CODEC_ID_MPEG2VIDEO:
    case    CODEC_ID_MPEG2VIDEO_XVMC:
      fprintf(stderr,"vcodec=MPEG2\n");
      return OMX_VIDEO_CodingMPEG2;
    case    CODEC_ID_H264:
      fprintf(stderr,"vcodec=AVC\n");
      return OMX_VIDEO_CodingAVC;
    case    13:
      fprintf(stderr,"vcodec=MPEG4\n");
      return OMX_VIDEO_CodingMPEG4;
    default:
      return -1;
  }

  return -1;
}

static int map_acodec(enum CodecID id)
{
  printf("Mapping audio codec ID %d (%x)\n", id, id);
  switch (id) {
    case    CODEC_ID_MP2:
    case    CODEC_ID_MP3:
      fprintf(stderr,"acodec=MPEG\n");
      return HMF_AUDIO_CODEC_MPEG;
    case    CODEC_ID_AC3:
      fprintf(stderr,"acodec=AC3\n");
      return HMF_AUDIO_CODEC_AC3;
    case    CODEC_ID_AAC:
      fprintf(stderr,"acodec=AAC\n");
      return HMF_AUDIO_CODEC_AAC;
    default:
      return -1;
  }

  return -1;
}

int avplay(struct codecs_t* codecs, const char* url)
{
  AVFormatContext *fmt_ctx = NULL;
  AVPacket pkt;
  int video_stream_idx = -1, audio_stream_idx = -1;
  int ret = 0;
  struct packet_t* packet;

  /* register all formats and codecs */
  av_register_all();

  /* open input file, and allocate format context */
  if (avformat_open_input(&fmt_ctx, url, NULL, NULL) < 0) {
    fprintf(stderr, "Could not open source file %s\n", url);
    return 1;
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    ret = 2;
    goto end;
  }

  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, url, 0);

  if ((ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) >= 0) {
    video_stream_idx = ret;
  }

  if ((ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0)) >= 0) {
    audio_stream_idx = ret;
  }

  if (audio_stream_idx == -1 && video_stream_idx == -1) {
    fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
    ret = 3;
    goto end;
  }

  fprintf(stderr,"video_stream_idx=%d\n",video_stream_idx);
  fprintf(stderr,"audio_stream_idx=%d\n",audio_stream_idx);

  if ((codecs->vcodec.vcodectype = map_vcodec(fmt_ctx->streams[video_stream_idx]->codec->codec_id)) == -1) {
    fprintf(stderr,"Unsupported video codec\n");
    exit(1);
  }
  if ((codecs->acodec.acodectype = map_acodec(fmt_ctx->streams[audio_stream_idx]->codec->codec_id)) == -1) {
    fprintf(stderr,"Unsupported audio codec\n");
    exit(1);
  }

  AVCodecContext* c =  fmt_ctx->streams[video_stream_idx]->codec;

  unsigned char* annexb_extradata = NULL;
  int annexb_extradata_size = 0;
  int nalsize = 0;

  if ((codecs->vcodec.vcodectype == OMX_VIDEO_CodingAVC) && (c->extradata)) {
    if ((c->extradata[0]==0) && (c->extradata[1]==0) && (c->extradata[2]==0) && (c->extradata[3]==1)) {
      fprintf(stderr,"Extradata is already in annexb format.\n");
      annexb_extradata = c->extradata;
      annexb_extradata_size = c->extradata_size;
    } else {
      int i,j,k;

      nalsize = (c->extradata[4] & 0x3) + 1;
      if (nalsize != 4) {
         fprintf(stderr,"Unsupported nalsize %d, aborting\n",nalsize);
         exit(1);
      }
      int sps_len = (c->extradata[6] << 8) | c->extradata[7];
      fprintf(stderr,"sps_len=%d\n",sps_len);
      annexb_extradata_size = sps_len + 4;

      int pps_count = c->extradata[8+sps_len];
      i = 9 + sps_len;
      for (j=0;j<pps_count;j++) {
        int pps_len = (c->extradata[i] << 8) | c->extradata[i+1];
        i += 2;
        fprintf(stderr,"pps_len=%d\n",pps_len);
        i += pps_len;
        annexb_extradata_size += 4 + pps_len;
      }

      fprintf(stderr,"annexb_extradata_size = %d\n",annexb_extradata_size);
      annexb_extradata = malloc(annexb_extradata_size);

      annexb_extradata[0] = 0;
      annexb_extradata[1] = 0;
      annexb_extradata[2] = 0;
      annexb_extradata[3] = 1;
      memcpy(annexb_extradata + 4, c->extradata+8, sps_len);
      k = sps_len + 4;
      i = 9 + sps_len;
      for (j=0;j<pps_count;j++) {
        int pps_len = (c->extradata[i] << 8) | c->extradata[i+1];
        i += 2;
        annexb_extradata[k] = 0;
        annexb_extradata[k+1] = 0;
        annexb_extradata[k+2] = 0;
        annexb_extradata[k+3] = 1;
        k += 4;
        memcpy(annexb_extradata + k, c->extradata+i, pps_len);
        i += pps_len;
      }
    }
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

#ifdef DUMP_VIDEO
  int fd;
  static int track = 0;
  char filename[32];

  sprintf(filename,"video%03d.dump",++track);
  fd = open(filename,O_CREAT|O_TRUNC|O_RDWR,0666);
  if (fd < 0) {
    fprintf(stderr,"Could not create video.dump\n");
    exit(1);
  }
#endif

  int first_video = 1; 
  AVRational omx_timebase = {1,1000000};
  /* read frames from the file */
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    fprintf(stderr,"Read pkt - index=%d\n",pkt.stream_index);

    if ((pkt.stream_index == video_stream_idx) || (pkt.stream_index == audio_stream_idx)) {
      packet = malloc(sizeof(*packet));
      packet->PTS = av_rescale_q(pkt.pts, fmt_ctx->streams[pkt.stream_index]->time_base, omx_timebase);

      packet->packetlength = pkt.size;

      if ((pkt.stream_index == video_stream_idx) && (first_video) && (annexb_extradata_size > 0)) {
        /* Add extradata to first video frame */
        packet->packetlength += annexb_extradata_size;
        packet->packet = malloc(packet->packetlength);
        memcpy(packet->packet, annexb_extradata, annexb_extradata_size);
        if (nalsize > 0) {
          convert4(packet->packet+annexb_extradata_size,pkt.data,pkt.size);
        } else {
          memcpy(packet->packet+annexb_extradata_size,pkt.data,pkt.size);
        }
      } else {
        packet->packet = malloc(packet->packetlength);
        if ((pkt.stream_index == video_stream_idx) && (nalsize > 0)) {
          convert4(packet->packet,pkt.data,pkt.size);
        } else {
          memcpy(packet->packet,pkt.data,pkt.size);
        }
      }
      packet->buf = packet->packet;  /* This is what is free()ed */
      
      if (pkt.stream_index == video_stream_idx) {
#ifdef DUMP_VIDEO
	write(fd,packet->packet,packet->packetlength);
#endif
	fprintf(stderr,"Adding video packet - PTS=%lld, size=%d\n",packet->PTS, packet->packetlength);
        first_video = 0;
        while (codecs->vcodec.queue_count > 100) { usleep(100000); }  // FIXME
	codec_queue_add_item(&codecs->vcodec,packet);
      } else {
	fprintf(stderr,"Adding audio packet - PTS=%lld, size=%d\n",packet->PTS, packet->packetlength);
        while (codecs->acodec.queue_count > 100) { usleep(100000); }  // FIXME
	codec_queue_add_item(&codecs->acodec,packet);
      }
    }
    
    av_free_packet(&pkt);
  }

#ifdef DUMP_VIDEO
  close(fd);
#endif

end:
  avformat_close_input(&fmt_ctx);

  return ret;
}
