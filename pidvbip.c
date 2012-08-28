#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>

#include "bcm_host.h"
#include "vcodec_mpeg2.h"
#include "vcodec_omx.h"
#include "acodec_mpeg.h"
#include "acodec_aac.h"
#include "htsp.h"
#include "channels.h"
#include "debug.h"

struct codecs_t {
  struct codec_t vcodec; // Video
  struct codec_t acodec; // Audio
  struct codec_t scodec; // Subtitles
  struct htsp_subscription_t subscription;  // Details of the currently tuned channel
};

/* TODO: Should this be global? */
struct htsp_t htsp;

/* The HTSP thread reads from the network and passes the incoming stream packets to the
   appropriate codec (video/audio/subtitle) */
void* htsp_receiver_thread(struct codecs_t* codecs)
{
  struct htsp_message_t msg;
  int res;
  struct packet_t* packet;
  int first_audio_packet = 1;

  while (((codecs->subscription.videostream != -1) && (codecs->vcodec.is_running)) || (codecs->acodec.is_running))
  {
    if ((res = htsp_recv_message(&htsp,&msg)) > 0) {
      fprintf(stderr,"FATAL ERROR in network read - %d\n",res);
      exit(1);
    }
    char* method = htsp_get_string(&msg,"method");

    int free_msg = 1;  // We want to free this message, unless this flag is set to zero

    if ((method != NULL) && (strcmp(method,"muxpkt")==0)) {
      int stream;
      htsp_get_int(&msg,"stream",&stream);
      if ((codecs->subscription.videostream != -1) && (stream==codecs->subscription.streams[codecs->subscription.videostream].index)) {
        if (first_audio_packet == 1) {
          //fprintf(stderr,"Dropping video packet before first audio packet\n");
          goto next;
	}
        packet = malloc(sizeof(*packet));
        packet->buf = msg.msg;

        int frametype;
        htsp_get_int(&msg,"frametype",&frametype);

	// htsp_dump_message(&msg);
        if (htsp_get_int64(&msg,"pts",&packet->PTS) > 0) {
          fprintf(stderr,"ERROR: No PTS in video packet, dropping\n");
          goto next;
        }
        if (htsp_get_int64(&msg,"dts",&packet->DTS) > 0) {
          fprintf(stderr,"ERROR: No DTS in video packet, dropping\n");
          goto next;
        }
        htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

        codec_queue_add_item(&codecs->vcodec,packet);
        free_msg = 0;   // Don't free this message

      } else if ((stream==codecs->subscription.streams[codecs->subscription.audiostream].index) &&
                 ((codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_MPEG) ||
                  (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AAC))
                ) {
        packet = malloc(sizeof(*packet));
        packet->buf = msg.msg;
        if (htsp_get_int64(&msg,"pts",&packet->PTS) > 0) {
          fprintf(stderr,"ERROR: No PTS in audio packet, dropping\n");
          goto next;
	}
        first_audio_packet = 0;
        htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

        codec_queue_add_item(&codecs->acodec,packet);
        free_msg = 0;   // Don't free this message
      }
    } else if (method != NULL) {
      //if (strcmp(method,"queueStatus") != 0)
      //  htsp_dump_message(&msg);
    }

    /* Temporary hack - don't display audio info unless it is a supported codec */
    if (codecs->subscription.streams[codecs->subscription.audiostream-1].codec == HMF_AUDIO_CODEC_MPEG) {
      fprintf(stderr,"v-queue: %8d packets, a-queue: %8d packets\r",codecs->vcodec.queue_count,codecs->acodec.queue_count);
    } else {
      fprintf(stderr,"v-queue: %8d packets\r",codecs->vcodec.queue_count);
    }

next:
    if (free_msg)
      htsp_destroy_message(&msg);

    if (method) free(method);
  }

  res = htsp_create_message(&msg,HMF_STR,"method","unsubscribe",HMF_S64,"subscriptionId",14,HMF_NULL);
  res = htsp_send_message(&htsp,&msg);
  htsp_destroy_message(&msg);

}

int main(int argc, char* argv[])
{
    int res;
    int channel = -1;
    int channel_id = -1;
    struct htsp_message_t msg;
    struct codecs_t codecs;
    pthread_t htspthread = 0;

    if ((argc != 3) && (argc != 4)) {
        fprintf(stderr,"Usage: pidvbip host port [channel num]\n");
        return 1;
    }

    bcm_host_init();

    if ((res = htsp_connect(&htsp,argv[1],atoi(argv[2]))) > 0) {
        fprintf(stderr,"Error connecting to htsp server, aborting.\n");
        return 2;
    }

    if (argc==4) { channel = atoi(argv[3]); }

    res = htsp_login(&htsp);

    res = htsp_create_message(&msg,HMF_STR,"method","enableAsyncMetadata",HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_destroy_message(&msg);

    // Recieve the acknowledgement from enableAsyncMetadata
    res = htsp_recv_message(&htsp,&msg);

    channels_init();

    struct termios new,orig;
    tcgetattr(0, &orig);
    memcpy(&new, &orig, sizeof(struct termios));
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VTIME] = 0;
    new.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new);

    int done = 0;
    while (!done)
    {
       // TODO: Store all received channels/tags/dvr data and stop when completed message received.
       res = htsp_recv_message(&htsp,&msg);

       char* method = htsp_get_string(&msg,"method");

       if (method) {
         if (strcmp(method,"initialSyncCompleted")==0) {
           done=1;
         } else if (strcmp(method,"channelAdd")==0) {
           // channelName, channelNumber, channelId
           int channelNumber,channelId;
           char* channelName;
           if (htsp_get_int(&msg,"channelId",&channelId) == 0) { 
             if (htsp_get_int(&msg,"channelNumber",&channelNumber) > 0) { channelNumber = 0; }
             channelName = htsp_get_string(&msg,"channelName");
             channels_add(channelNumber,channelId,channelName);
           }
         } else {
           //fprintf(stderr,"Recieved message: method=\"%s\"\n",method);
           //htsp_dump_message(&msg);
         }

         free(method);
       }

       htsp_destroy_message(&msg);
    }

    fprintf(stderr,"Initial sync completed\n");

    if (channels_getcount() == 0) {
      fprintf(stderr,"No channels available, exiting.\n");
      exit(1);
    }

    channels_dump();

    channel_id = channels_getid(channel);
    if (channel_id < 0)
      channel_id = channels_getfirst();

next_channel:
    memset(&codecs.vcodec,0,sizeof(codecs.vcodec));
    memset(&codecs.acodec,0,sizeof(codecs.acodec));

    fprintf(stderr,"Tuning to channel %d - \"%s\"                        \n",channels_getlcn(channel_id),channels_getname(channel_id));

    res = htsp_create_message(&msg,HMF_STR,"method","subscribe",HMF_S64,"channelId",channel_id,HMF_S64,"subscriptionId",14,HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_destroy_message(&msg);

    res = htsp_recv_message(&htsp,&msg);

    //    if () {
    //      fprintf(stderr,"Tuning failed - wait for next channel\n");
    //      goto wait_for_key;
    //    }
    
    char* method = htsp_get_string(&msg,"method");
    while ((method == NULL) || (strcmp(method,"subscriptionStart")!=0)) {
      if (method != NULL) {
        // TODO: Process any messages recieved whilst waiting for subscriptionStart
        free(method);
      }

      char* status = htsp_get_string(&msg,"status");
      htsp_destroy_message(&msg);

      if ((status != NULL) && (strcmp(status,"OK")!=0)) {
        fprintf(stderr,"Tuning error: %s\n",status);
        free(status);
        goto wait_for_key;
      }

      res = htsp_recv_message(&htsp,&msg);
      method = htsp_get_string(&msg,"method");
    }
    if (method != NULL) free(method);

    // We have received the subscriptionStart message, now parse it
    // to get the stream info    

    if (htsp_parse_subscriptionStart(&msg,&codecs.subscription) > 0) {
      fprintf(stderr,"FATAL ERROR: Cannot parse subscriptionStart\n");
      exit(1);
    }

    htsp_destroy_message(&msg);

    if (codecs.subscription.videostream != -1) {
      if (codecs.subscription.streams[codecs.subscription.videostream].codec == HMF_VIDEO_CODEC_MPEG2) {
#ifdef SOFTWARE_MPEG2
        vcodec_mpeg2_init(&codecs.vcodec);
#else
        vcodec_omx_init(&codecs.vcodec,OMX_VIDEO_CodingMPEG2);
#endif
      } else if (codecs.subscription.streams[codecs.subscription.videostream].codec == HMF_VIDEO_CODEC_H264) {
        vcodec_omx_init(&codecs.vcodec,OMX_VIDEO_CodingAVC);
      } else {
        fprintf(stderr,"UNKNOWN VIDEO FORMAT\n");
        exit(1);
      }

      codecs.vcodec.acodec = &codecs.acodec;
    }

    if (codecs.subscription.streams[codecs.subscription.audiostream].codec == HMF_AUDIO_CODEC_MPEG) {
      acodec_mpeg_init(&codecs.acodec);
      DEBUGF("Initialised mpeg codec\n");
    } else if (codecs.subscription.streams[codecs.subscription.audiostream].codec == HMF_AUDIO_CODEC_AAC) {
      acodec_aac_init(&codecs.acodec);
      DEBUGF("Initialised AAC codec\n");
    }

    // TODO: Audio and subtitle threads


    // All codec threads are running, so now start receiving data
    pthread_create(&htspthread,NULL,(void * (*)(void *))htsp_receiver_thread,(void*)&codecs);

    /* UI loop - just block on keyboad input for now... */
wait_for_key:
    while (1) {
      int c = getchar();
      DEBUGF("\n char read: 0x%08x ('%c')\n", c,(isalnum(c) ? c : ' '));
      if (c=='q') goto done;
      if ((c=='n') || (c=='p')) {
        if (codecs.vcodec.thread) {
          codec_stop(&codecs.vcodec);
          pthread_join(codecs.vcodec.thread,NULL);
        }

        if (codecs.acodec.thread) {
          codec_stop(&codecs.acodec);
          pthread_join(codecs.acodec.thread,NULL);
        }

        /* Wait for htsp receiver thread to stop */
        DEBUGF("Wait for receiver thread to stop.\n");

        if (htspthread) {
          pthread_join(htspthread,NULL);
          htspthread = 0;
          DEBUGF("Receiver thread stopped.\n");
        }

        if (c=='n') channel_id = channels_getnext(channel_id);
        else channel_id = channels_getprev(channel_id);

        goto next_channel;
      }
    }

done:
    tcsetattr(0, TCSANOW, &orig);
    return 0;
}
