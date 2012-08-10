#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "vcodec_mpeg2.h"
#include "vcodec_h264.h"
#include "htsp.h"

int main(int argc, char* argv[])
{
    int res;
    int channel;
    struct htsp_t htsp;
    struct htsp_message_t msg;
    struct packet_t* video_packet;
    struct codec_t vcodec;

    if (argc != 4) {
        fprintf(stderr,"Usage: htsptest host port channelId\n");
        return 1;
    }

    vcodec_h264_init(&vcodec);

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

    while (1) {
       // TODO: Parse all updates and add video packets to mpeg2 queue
       // Put this in its own thread.
       res = htsp_recv_message(&htsp,&msg);

       char* method = htsp_get_string(&msg,"method");

       if ((method != NULL) && (strcmp(method,"muxpkt")==0)) {
          int stream;
          htsp_get_int(&msg,"stream",&stream);
          if (stream==1) {  // To Do: Is video always stream 1?  Maybe also check subscriptionId
            video_packet = malloc(sizeof(*video_packet));
            video_packet->buf = msg.msg;
            htsp_get_bin(&msg,"payload",&video_packet->packet,&video_packet->packetlength);

            // TODO: Populate PTS and DTS
            //fprintf(stderr,"Adding video packet to queue\n");
            codec_queue_add_item(&vcodec,video_packet);
            fprintf(stderr,"Queue count:  %8d\r",vcodec.queue_count);
            //htsp_destroy_message(&msg);
          } else {
            htsp_destroy_message(&msg);
          }
       } else {
          //htsp_dump_message(&msg);
          htsp_destroy_message(&msg);
       }
       if (method) free(method);
    }

    return 0;
}
