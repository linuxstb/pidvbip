#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>

#include "vcodec_mpeg2.h"
#include "vcodec_h264.h"
#include "acodec_mpeg.h"
#include "htsp.h"

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

  while (1)
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
      if (stream==codecs->subscription.videostream) {
        packet = malloc(sizeof(*packet));
        packet->buf = msg.msg;
        htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

        // TODO: Populate PTS and DTS
        //fprintf(stderr,"Adding video packet to queue\n");
        codec_queue_add_item(&codecs->vcodec,packet);
        free_msg = 0;   // Don't free this message
        fprintf(stderr,"Queue count:  %8d\r",codecs->vcodec.queue_count);
      } else if ((stream==codecs->subscription.audiostream) &&
                 (codecs->subscription.streams[codecs->subscription.audiostream-1].codec == HMF_AUDIO_CODEC_MPEG)) {
        packet = malloc(sizeof(*packet));
        packet->buf = msg.msg;
        htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

        codec_queue_add_item(&codecs->acodec,packet);
        free_msg = 0;   // Don't free this message
      }
    } else {
      //htsp_dump_message(&msg);
    }

    if (free_msg)
      htsp_destroy_message(&msg);

    if (method) free(method);
  }
}


int main(int argc, char* argv[])
{
    int res;
    int channel;
    struct htsp_message_t msg;
    struct codecs_t codecs;

    if (argc != 4) {
        fprintf(stderr,"Usage: htsptest host port channelId\n");
        return 1;
    }

    if ((res = htsp_connect(&htsp,argv[1],atoi(argv[2]))) > 0) {
        fprintf(stderr,"Error connecting to htsp server, aborting.\n");
        return 2;
    }

    channel = atoi(argv[3]);
    res = htsp_login(&htsp);

    res = htsp_create_message(&msg,HMF_STR,"method","enableAsyncMetadata",HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_destroy_message(&msg);

    // Recieve the acknowledgement from enableAsyncMetadata
    res = htsp_recv_message(&htsp,&msg);

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
             fprintf(stderr,"%5d  %5d - %s\n",channelId,channelNumber,channelName);
             free(channelName);
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

    res = htsp_create_message(&msg,HMF_STR,"method","subscribe",HMF_S64,"channelId",channel,HMF_S64,"subscriptionId",14,HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_destroy_message(&msg);

    
    res = htsp_recv_message(&htsp,&msg);
    char* method = htsp_get_string(&msg,"method");
    while ((method == NULL) || (strcmp(method,"subscriptionStart")!=0)) {
      if (method != NULL) {
        // TODO: Process any messages recieved whilst waiting for subscriptionStart
        free(method);
      }
      htsp_destroy_message(&msg);

      res = htsp_recv_message(&htsp,&msg);
      method = htsp_get_string(&msg,"method");
    }
    htsp_destroy_message(&msg);
    if (method != NULL) free(method);

    // We have received the subscriptionStart message, now parse it
    // to get the stream info    

    if (htsp_parse_subscriptionStart(&msg,&codecs.subscription) > 0) {
      fprintf(stderr,"FATAL ERROR: Cannot parse subscriptionStart\n");
      exit(1);
    }

    if (codecs.subscription.streams[codecs.subscription.videostream-1].codec == HMF_VIDEO_CODEC_MPEG2) {
      vcodec_mpeg2_init(&codecs.vcodec);
    } else if (codecs.subscription.streams[codecs.subscription.videostream-1].codec == HMF_VIDEO_CODEC_H264) {
      vcodec_h264_init(&codecs.vcodec);
    } else {
      fprintf(stderr,"UNKNOWN VIDEO FORMAT\n");
      exit(1);
    }

    if (codecs.subscription.streams[codecs.subscription.audiostream-1].codec == HMF_AUDIO_CODEC_MPEG) {
      acodec_mpeg_init(&codecs.acodec);
    }

    // TODO: Audio and subtitle threads


    // All codec threads are running, so now start receiving data
    pthread_t htspthread;
    pthread_create(&htspthread,NULL,(void * (*)(void *))htsp_receiver_thread,(void*)&codecs);

    /* UI loop - just block on keyboad input for now... */
    struct termios new,orig;
    tcgetattr(0, &orig);
    memcpy(&new, &orig, sizeof(struct termios));
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VTIME] = 0;
    new.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new);

    int c;
    while (1) {
      c = getchar();
      printf("\n char read: 0x%08x ('%c')\n", c,(isalnum(c) ? c : ' '));
      if (c=='q') goto done;
    }

done:
    tcsetattr(0, TCSANOW, &orig);
    return 0;
}
