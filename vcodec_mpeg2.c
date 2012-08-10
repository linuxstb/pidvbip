
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "libmpeg2/mpeg2.h"
#include "vcodec_mpeg2.h"
#include "vo_pi.h"

int enable_output = 1;

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

static void* vcodec_mpeg2_thread(struct vcodec_mpeg2_t* decoder)
{
  const mpeg2_sequence_t * sequence;
  mpeg2_state_t state;
  int nframes = 0;

  const mpeg2_info_t * info = mpeg2_info(decoder->decoder);

  while (1) {
        state = mpeg2_parse (decoder->decoder);
        sequence = info->sequence;
        switch (state) {
        case STATE_SEQUENCE:
            fprintf(stderr,"SEQUENCE: nframes=%d\n",nframes);
            fprintf(stderr,"Video is %d x %d\n",sequence->width,sequence->height);
            int pitch = ALIGN_UP(sequence->width,32);

            int i;
            for (i = 0; i < 3 ; i++) {
              uint8_t* buf[3];

              fbuf[i].data = calloc((3 * pitch * sequence->height) / 2,1);
              buf[0] = fbuf[i].data;
              buf[1] = buf[0] + pitch * sequence->height;
              buf[2] = buf[1] + (pitch >> 1) * (sequence->height >> 1);

              mpeg2_set_buf(decoder->decoder, buf, fbuf );  
            }
            mpeg2_stride(decoder->decoder,pitch);
            fprintf(stderr,"Stride set to %d\n",pitch);
            break;
        case STATE_BUFFER:
	  //            fprintf(stderr,"[vcodec_mpeg2] STATE_BUFFER\n");
            pthread_mutex_lock(&decoder->queue_mutex);

            if (decoder->current != NULL) {
               // TODO: Free 
	      if (decoder->current != decoder->queue_tail) {
                fprintf(stderr,"Error - current is not equal to queue_tail\n");
                exit(1);
              }
              decoder->queue_tail = decoder->queue_tail->prev;
              if (decoder->queue_tail == NULL) {
                decoder->queue_head = NULL;
              } else {
                decoder->queue_tail->next = NULL;
              }
              free(decoder->current->data->buf);
              free(decoder->current->data);
              free(decoder->current);
              decoder->queue_count--;
            }

            if (decoder->queue_tail == NULL) {
	      pthread_cond_wait(&decoder->queue_count_cv,&decoder->queue_mutex);
            }

            mpeg2_buffer(decoder->decoder,decoder->queue_tail->data->packet,decoder->queue_tail->data->packet + decoder->queue_tail->data->packetlength);
            decoder->current = decoder->queue_tail;
            pthread_mutex_unlock(&decoder->queue_mutex);
            break;
        case STATE_SLICE:
        case STATE_END:
        case STATE_INVALID_END:
            if ((enable_output) && (info->display_fbuf)) {
              //fprintf(stderr,"[vcodec_mpeg2] Displaying frame\n");
              double frame_period = (sequence->frame_period/27000000.0)*1000000.0;
              double now = gettime();
              if (decoder->nextframetime < 0) {
                decoder->nextframetime = now + frame_period;
              } else {
                if (now < decoder->nextframetime) {
                  usleep(decoder->nextframetime - now);
                }
                decoder->nextframetime += frame_period;
              }
              vo_display_frame (&decoder->vars, sequence->width, sequence->height,
                                sequence->chroma_width, sequence->chroma_height,
                                info->display_fbuf->buf, nframes);
              nframes++;
            }
            break;
        default:
            break;
        }
    }
}

void vcodec_mpeg2_init(struct vcodec_mpeg2_t* decoder)
{
  mpeg2_accel(1);
  decoder->decoder = mpeg2_init ();

  if (decoder->decoder == NULL) {
    fprintf (stderr, "Could not allocate a decoder object.\n");
    exit(1);
  }

  decoder->current = NULL;
  decoder->queue_head = NULL;
  decoder->queue_tail = NULL;
  decoder->queue_count = 0;

  decoder->nextframetime = -1.0;

  vo_open(&decoder->vars,0);

  pthread_mutex_init(&decoder->queue_mutex,NULL);
  pthread_cond_init(&decoder->queue_count_cv,NULL);

  int res = pthread_create(&decoder->thread,NULL,(void * (*)(void *))vcodec_mpeg2_thread,(void*)decoder);
}


void vcodec_mpeg2_add_to_queue(struct vcodec_mpeg2_t* decoder, struct mpeg2_packet_t* packet)
{
  struct decoder_queue_t* new = malloc(sizeof(struct decoder_queue_t));

  if (new == NULL) {
    fprintf(stderr,"FATAL ERROR: out of memory adding to queue\n");
    exit(1);
  }

  pthread_mutex_lock(&decoder->queue_mutex);

  if (decoder->queue_head == NULL) {
    new->next = NULL;
    new->prev = NULL;
    new->data = packet;
    decoder->queue_head = new;
    decoder->queue_tail = new;

    pthread_cond_signal(&decoder->queue_count_cv);
  } else {
    new->data = packet;
    new->next = decoder->queue_head;
    new->prev = NULL;
    new->next->prev = new;
    decoder->queue_head = new;
  }

  decoder->queue_count++;

  pthread_mutex_unlock(&decoder->queue_mutex);
}

int64_t vcodec_mpeg2_current_get_pts(struct vcodec_mpeg2_t* decoder)
{
}
