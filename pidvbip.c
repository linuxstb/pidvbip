#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <sys/time.h>
#include <interface/vmcs_host/vcgencmd.h>

#include "bcm_host.h"
#include "vcodec_mpeg2.h"
#include "vcodec_omx.h"
#include "acodec_mpeg.h"
#include "acodec_aac.h"
#include "acodec_a52.h"
#include "htsp.h"
#include "channels.h"
#include "events.h"
#include "debug.h"
#include "osd.h"
#include "avahi.h"

struct codecs_t {
  struct codec_t vcodec; // Video
  struct codec_t acodec; // Audio
  struct codec_t scodec; // Subtitles
  struct htsp_subscription_t subscription;  // Details of the currently tuned channel
};

/* TODO: Should this be global? */
struct htsp_t htsp;

int hw_mpeg2;

int mpeg2_codec_enabled(void)
{
  char response[1024];

  vc_gencmd_init();

  vc_gencmd(response,sizeof(response), "codec_enabled MPG2");

  if (strcmp(response,"MPG2=enabled")==0) {
    return 1;
  } else {
    return 0;
  }
}

/* Enable to dump video streams to files (for debugging) */
//#define DUMP_VIDEO

void process_message(char* method,struct htsp_message_t* msg,char* debugtext)
{
  if ((strcmp(method,"eventAdd")==0) || (strcmp(method,"eventUpdate")==0)) {
    process_event_message(method,msg);
  } else if (strcmp(method,"eventDeleted")==0) {
    uint32_t eventId;
    if (htsp_get_uint(msg,"eventId",&eventId)==0) {
      fprintf(stderr,"eventDeleted: %d\n",eventId);
      event_delete(eventId);
    } else {
      fprintf(stderr,"Warning eventDeleted event not found (%d)\n",eventId);
    }
  } else if ((strcmp(method,"channelAdd")==0) || (strcmp(method,"channelUpdate")==0)) {
    // channelName, channelNumber, channelId
    int channelNumber,channelId,channelType;
    uint32_t eventid,nexteventid;
    char* channelName;
    unsigned char* list;
    int listlen;

    if (htsp_get_int(msg,"channelId",&channelId) == 0) { 
      if (htsp_get_int(msg,"channelNumber",&channelNumber) > 0) { channelNumber = -1; }
      if (htsp_get_uint(msg,"eventId",&eventid) > 0) { eventid = 0; }
      if (htsp_get_uint(msg,"nextEventId",&nexteventid) > 0) { nexteventid = 0; }
      channelName = htsp_get_string(msg,"channelName");

      if (htsp_get_list(msg,"services",&list,&listlen) > 0)
      {
        channelType = 0;
      } else {
        unsigned char* buf = list;
        int type = buf[0]; if (type > 6) { type = 0; }
        int namelength = buf[1];
        int datalength = (buf[2] << 24) | (buf[3] << 16) | (buf[4] << 8) | buf[5];

        buf += 6;

        struct htsp_message_t tmpmsg;
        tmpmsg.msg = buf + namelength - 4;
        tmpmsg.msglen = datalength;

        char* typestr = htsp_get_string(&tmpmsg,"type");
        if (strncmp(typestr,"SDTV",4)==0) {
          //fprintf(stderr,"Type=SDTV\n");
          channelType = CTYPE_SDTV;
        } else if (strncmp(typestr,"HDTV",4)==0) {
          //fprintf(stderr,"Type=HDTV\n");
          channelType = CTYPE_HDTV;
        } else if (strncmp(typestr,"Radio",4)==0) {
          //fprintf(stderr,"Type=RADIO\n");
          channelType = CTYPE_RADIO;
        } else {
          fprintf(stderr,"Type=%s (unknown - defaulting to SDTV\n",typestr);
          channelType = CTYPE_SDTV;
        }
        free(typestr);
      }

      if (strcmp(method,"channelAdd")==0) {
        channels_add(channelNumber,channelId,channelName,channelType,eventid,nexteventid);
        //fprintf(stderr,"channelAdd - id=%d,lcn=%d,name=%s,current_event=%d,next_event=%d\n",channelId,channelNumber,channelName,eventid,nexteventid);
      } else {
        channels_update(channelNumber,channelId,channelName,channelType,eventid,nexteventid);
        //fprintf(stderr,"channelUpdate - id=%d,current_event=%d,next_event=%d\n",channelId,eventid,nexteventid);
      }
    }
  } else if (strcmp(method,"queueStatus")== 0) {
    /* Are we interested? */
  } else if (strcmp(method,"signalStatus")== 0) {
    /* Are we interested? */
  } else {
    fprintf(stderr,"%s: Received message %s\n",debugtext,method);
    //htsp_dump_message(msg);
  }
}

/* The HTSP thread reads from the network and passes the incoming stream packets to the
   appropriate codec (video/audio/subtitle) */
void* htsp_receiver_thread(struct codecs_t* codecs)
{
  struct htsp_message_t msg;
  int res;
  struct packet_t* packet;
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

  while (((codecs->subscription.videostream != -1) && (codec_is_running(&codecs->vcodec))) || (codec_is_running(&codecs->acodec)))
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

#ifdef DUMP_VIDEO
        write(fd,packet->packet,packet->packetlength);
#endif

        codec_queue_add_item(&codecs->vcodec,packet);
        free_msg = 0;   // Don't free this message

      } else if ((stream==codecs->subscription.streams[codecs->subscription.audiostream].index) &&
                 ((codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_MPEG) ||
                  (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AC3) ||
                  (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AAC))
                ) {
        packet = malloc(sizeof(*packet));
        packet->buf = msg.msg;
        if (htsp_get_int64(&msg,"pts",&packet->PTS) > 0) {
          fprintf(stderr,"ERROR: No PTS in audio packet, dropping\n");
          goto next;
        }
        htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

        codec_queue_add_item(&codecs->acodec,packet);
        free_msg = 0;   // Don't free this message
      }
    } else if (method != NULL) {
      process_message(method,&msg,"htsp_receiver_thread");
    }

    //fprintf(stderr,"v-queue: %8d (%d) packets, a-queue: %8d (%d) packets\r",codecs->vcodec.queue_count,codecs->vcodec.is_running,codecs->acodec.queue_count,codecs->acodec.is_running);

next:
    if (free_msg)
      htsp_destroy_message(&msg);

    if (method) free(method);
  }

#ifdef DUMP_VIDEO
  close(fd);
#endif
  res = htsp_create_message(&msg,HMF_STR,"method","unsubscribe",HMF_S64,"subscriptionId",14,HMF_NULL);
  res = htsp_send_message(&htsp,&msg);
  htsp_destroy_message(&msg);

  return 0;
}

int read_config(char* configfile,char** host, int* port)
{
  int fd = -1;
  int res;
  char buf[1024];

  if (configfile)
    fd = open(configfile,O_RDONLY);

  if (fd < 0)
    fd = open("/boot/pidvbip.txt",O_RDONLY);  /* FAT partition in Raspbian */

  if (fd < 0)
    fd = open("/flash/pidvbip.txt",O_RDONLY); /* FAT partition in OpenELEC */

  if (fd < 0) {
    fprintf(stderr,"Could not open config file\n");
    return -1;
  }

  res = read(fd, buf, sizeof(buf)-1);
  buf[1023] = 0;
  close(fd);

  if (res < 0) {
    fprintf(stderr,"Error reading from config file\n");
    return -1;
  }

  *host = malloc(1024);
  res = sscanf(buf,"%s %d",*host,port);

  if (res != 2) {
    fprintf(stderr,"Error parsing config file\n");
    return -1;
  }

  return 0;
}

void usage(void)
{
  fprintf(stderr,"pidvbip - tvheadend client for the Raspberry Pi\n");
  fprintf(stderr,"\nOptions:\n\n");
}

static int get_actual_channel(int auto_hdtv, int user_channel_id)
{
  int actual_channel_id = user_channel_id;

  if ((auto_hdtv) && (channels_gettype(user_channel_id)==CTYPE_SDTV)) {
    uint32_t current_eventId = channels_geteventid(user_channel_id);
    int hd_channel = event_find_hd_version(current_eventId);
    if (hd_channel >= 0) {
      fprintf(stderr,"Auto-switching to channel %d (%s)\n",channels_getlcn(hd_channel),channels_getname(hd_channel));
      actual_channel_id = hd_channel;
    }
  }

  return actual_channel_id;
}

double get_time(void)
{
  struct timeval tv;

  gettimeofday(&tv,NULL);

  double x = tv.tv_sec;
  x *= 1000;
  x += tv.tv_usec / 1000;

  return x;
}

int main(int argc, char* argv[])
{
    int res;
    int channel = -1;
    int user_channel_id = -1;
    int actual_channel_id = -1;
    int auto_hdtv = 0;
    struct htsp_message_t msg;
    struct codecs_t codecs;
    struct osd_t osd;
    pthread_t htspthread = 0;
    double osd_cleartime = 0;

    htsp.host = NULL;

    if (argc == 1) {
      /* No arguments, try avahi discovery */
      avahi_discover_tvh(&htsp);

      if (htsp.host == NULL) {
        /* No avahi, try to read default config from /boot/config.txt */
        if (read_config(NULL,&htsp.host,&htsp.port) < 0) {
          fprintf(stderr,"ERROR: Could not read from config file\n");
          fprintf(stderr,"Create config.txt in /boot/pidvbip.txt containing one line with the\n");
          fprintf(stderr,"host and port of the server separated by a space.\n");
          exit(1);
        }
      }
//    } else if (argc==2) {
//      /* One argument - config file */
    } else if ((argc != 3) && (argc != 4)) {
        usage();
        return 1;
    } else {
        htsp.host = argv[1];
        htsp.port = atoi(argv[2]);
    }

    fprintf(stderr,"Using host \"%s:%d\"\n",htsp.host,htsp.port);
    bcm_host_init();

    hw_mpeg2 = mpeg2_codec_enabled();

    if (hw_mpeg2) {
      fprintf(stderr,"Using hardware MPEG-2 decoding\n");
    } else {
      fprintf(stderr,"Using software MPEG-2 decoding\n");
    }

    osd_init(&osd);

    if ((res = htsp_connect(&htsp)) > 0) {
        fprintf(stderr,"Error connecting to htsp server, aborting.\n");
        return 2;
    }

    if (argc==4) { channel = atoi(argv[3]); }

    res = htsp_login(&htsp);

    if (res > 0) {
      fprintf(stderr,"Could not login to server\n");
      return 3;
    }
    res = htsp_create_message(&msg,HMF_STR,"method","enableAsyncMetadata",HMF_S64,"epg",1,HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_destroy_message(&msg);

    // Receive the acknowledgement from enableAsyncMetadata
    res = htsp_recv_message(&htsp,&msg);

    channels_init();
    events_init();

    struct termios new,orig;
    tcgetattr(0, &orig);
    memcpy(&new, &orig, sizeof(struct termios));
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VTIME] = 0;
    new.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new);

    int num_events = 0;
    int done = 0;
    while (!done)
    {
       // TODO: Store all received channels/tags/dvr data and stop when completed message received.
       res = htsp_recv_message(&htsp,&msg);

       char* method = htsp_get_string(&msg,"method");

       if (method) {
         if (strcmp(method,"initialSyncCompleted")==0) {
           done=1;
         } else {
           process_message(method,&msg,"Initial sync:");
         }

         free(method);
       }

       htsp_destroy_message(&msg);
    }

    fprintf(stderr,"Initial sync completed - read data for %d events\n",num_events);

    if (channels_getcount() == 0) {
      fprintf(stderr,"No channels available, exiting.\n");
      exit(1);
    }

    channels_dump();

    user_channel_id = channels_getid(channel);
    if (user_channel_id < 0)
      user_channel_id = channels_getfirst();

    actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);

    memset(&codecs.vcodec,0,sizeof(codecs.vcodec));
    memset(&codecs.acodec,0,sizeof(codecs.acodec));

next_channel:
    if (codecs.vcodec.thread) {
      codec_stop(&codecs.vcodec);
      pthread_join(codecs.vcodec.thread,NULL);
    }

    if (codecs.acodec.thread) {
      codec_stop(&codecs.acodec);
      pthread_join(codecs.acodec.thread,NULL);
    }

    /* Wait for htsp receiver thread to stop */

    if (htspthread) {
      DEBUGF("Wait for receiver thread to stop.\n");
      pthread_join(htspthread,NULL);
      htspthread = 0;
      DEBUGF("Receiver thread stopped.\n");
    }

    memset(&codecs.vcodec,0,sizeof(codecs.vcodec));
    memset(&codecs.acodec,0,sizeof(codecs.acodec));
    
    fprintf(stderr,"Tuning to channel %d - \"%s\"\n",channels_getlcn(user_channel_id),channels_getname(user_channel_id));

    osd_show_info(&osd,user_channel_id);
    osd_cleartime = get_time() + 5000;

    res = htsp_create_message(&msg,HMF_STR,"method","subscribe",HMF_S64,"channelId",actual_channel_id,HMF_S64,"subscriptionId",14,HMF_NULL);
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
        process_message(method,&msg,"subscriptionStart_loop");
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
        if (hw_mpeg2) {
          vcodec_omx_init(&codecs.vcodec,OMX_VIDEO_CodingMPEG2);
        } else {
          vcodec_mpeg2_init(&codecs.vcodec);
        }
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
    } else if (codecs.subscription.streams[codecs.subscription.audiostream].codec == HMF_AUDIO_CODEC_AC3) {
      acodec_a52_init(&codecs.acodec);
      DEBUGF("Initialised A/52 codec\n");
    }

    // TODO: Audio and subtitle threads


    // All codec threads are running, so now start receiving data
    pthread_create(&htspthread,NULL,(void * (*)(void *))htsp_receiver_thread,(void*)&codecs);

    /* UI loop - just block on keyboad input for now... */
    int new_channel;
    double new_channel_timeout;
wait_for_key:
    new_channel = -1;
    new_channel_timeout = 0;
    while (1) {
      int c;
      struct timeval tv = { 0L, 100000L };  /* 100ms */
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(0, &fds);
      if (select(1, &fds, NULL, NULL, &tv)==0) {
        c = -1;
      } else {
        c = getchar();
        DEBUGF("\n char read: 0x%08x ('%c')\n", c,(isalnum(c) ? c : ' '));

        switch (c) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            if (new_channel == -1) {
              new_channel = c - '0';
            } else {
              new_channel = (new_channel * 10) + (c - '0');
              /* Limit to 4 digits */
              if (new_channel > 10000) {
                new_channel = new_channel % 10000;
              }
            }
            if (osd_cleartime) {
              osd_clear(&osd);
              osd_cleartime = 0;
            }
            osd_show_newchannel(&osd,new_channel);
            new_channel_timeout = get_time() + 1000;
            break;

          case 'q':
            goto done;

          case 'i':
            osd_show_info(&osd,user_channel_id);
            osd_cleartime = get_time() + 5000;

            break;

          case 'h':
            auto_hdtv = 1 - auto_hdtv;
            int new_actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
            if (new_actual_channel_id != actual_channel_id) {
              actual_channel_id = new_actual_channel_id;
              goto next_channel;
            }
            break;

          case 'n':
          case 'p':
            if (c=='n') user_channel_id = channels_getnext(user_channel_id);
            else user_channel_id = channels_getprev(user_channel_id);
            actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);

            goto next_channel;

          default:
            break;            
        }
      }

      if ((osd_cleartime) && (get_time() > osd_cleartime)) {
        osd_clear(&osd);
        osd_cleartime = 0;
      }

      if ((new_channel_timeout) && (get_time() >= new_channel_timeout)) {
        fprintf(stderr,"new_channel = %d\n",new_channel);
        int new_channel_id = channels_getid(new_channel);
        if (new_channel_id >= 0) {
          user_channel_id = new_channel_id;
          actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
          goto next_channel;
        } else {
          osd_clear_newchannel(&osd);
          fprintf(stderr,"No such channel\n");
          new_channel = -1;
          new_channel_timeout = 0;
        }
      }
    }

done:
    tcsetattr(0, TCSANOW, &orig);
    return 0;
}
