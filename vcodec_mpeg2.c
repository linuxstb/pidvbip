
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "libmpeg2/mpeg2.h"
#include "codec.h"
#include "vcodec_mpeg2.h"
#include "vo_pi.h"
#include "debug.h"

int enable_output = 1;

#define LOW32(x) ((int64_t)(x) & 0xffffffff)
#define HIGH32(x) (((int64_t)(x) & 0xffffffff00000000) >> 32)


static struct fbuf_t
{
    uint8_t *data;
} fbuf[3];

static double gettime(void)
{
    struct timeval tv;
    double x;

    gettimeofday(&tv,NULL);
    x = tv.tv_sec;
    x *= 1000000.0;
    x += tv.tv_usec;

    return x;
}

static void* vcodec_mpeg2_thread(struct codec_t* codec)
{
  const mpeg2_sequence_t * sequence;
  mpeg2_state_t state;
  int nframes = 0;
  mpeg2dec_t* decoder = (mpeg2dec_t*)codec->codecstate;
  struct codec_queue_t* current = NULL;

  const mpeg2_info_t * info = mpeg2_info(decoder);

  while (1) {
        state = mpeg2_parse (decoder);
        sequence = info->sequence;
        switch (state) {
        case STATE_SEQUENCE:
	    DEBUGF("SEQUENCE: nframes=%d\n",nframes);
	    DEBUGF("Video is %d x %d\n",sequence->width,sequence->height);
            int pitch = VO_ALIGN_UP(sequence->width,32);

            int i;
            for (i = 0; i < 3 ; i++) {
              uint8_t* buf[3];

              fbuf[i].data = calloc((3 * pitch * sequence->height) / 2,1);
              buf[0] = fbuf[i].data;
              buf[1] = buf[0] + pitch * sequence->height;
              buf[2] = buf[1] + (pitch >> 1) * (sequence->height >> 1);

              mpeg2_set_buf(decoder, buf, fbuf );  
            }
            mpeg2_stride(decoder,pitch);
            DEBUGF("Stride set to %d\n",pitch);
            break;
        case STATE_BUFFER:
            codec_queue_free_item(codec,current);

            current = codec_queue_get_next_item(codec);

            if (current->msgtype == MSG_STOP) {
              DEBUGF("[vcodec_mpeg2] Stopping\n");
              codec_queue_free_item(codec,current);
              goto stop;
            }

            mpeg2_tag_picture(decoder,LOW32(current->data->PTS),HIGH32(current->data->PTS));

            mpeg2_buffer(decoder,current->data->packet,current->data->packet + current->data->packetlength);
            break;
        case STATE_SLICE:
        case STATE_END:
        case STATE_INVALID_END:
            if ((enable_output) && (info->display_fbuf)) {
              //fprintf(stderr,"[vcodec_mpeg2] Displaying frame\n");
              int64_t PTS = info->display_picture->tag2;
              PTS <<= 32;
              PTS |= info->display_picture->tag;

              int64_t audio_PTS = codec_get_pts(codec->acodec);

	      //fprintf(stderr,"Displaying frame - video PTS=%lld, audio PTS = %lld\n",PTS,audio_PTS);

              if (PTS > audio_PTS)
		usleep(PTS - audio_PTS);

              codec_set_pts(codec, PTS);
         
              vo_display_frame (&codec->vars, sequence->width, sequence->height,
                                sequence->chroma_width, sequence->chroma_height,
                                info->display_fbuf->buf, nframes);
              nframes++;
            }
            break;
        default:
            break;
        }
    }

stop:

  mpeg2_close(decoder);
  vo_close(&codec->vars);

  return NULL;
}

void vcodec_mpeg2_init(struct codec_t* codec)
{
  mpeg2_accel(1);
  codec->codecstate = mpeg2_init ();

  if (codec->codecstate == NULL) {
    fprintf (stderr, "Could not allocate a decoder object.\n");
    exit(1);
  }

  codec_queue_init(codec);

  vo_open(&codec->vars,0);

  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_mpeg2_thread,(void*)codec);
}
