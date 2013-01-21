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
#include <linux/input.h>
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
#include "cec.h"

struct codecs_t {
  struct codec_t vcodec; // Video
  struct codec_t acodec; // Audio
  struct codec_t scodec; // Subtitles
  struct htsp_subscription_t subscription;  // Details of the currently tuned channel
  int is_paused;
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
  } else if (strcmp(method,"eventDelete")==0) {
    uint32_t eventId;
    if (htsp_get_uint(msg,"eventId",&eventId)==0) {
      //fprintf(stderr,"eventDelete: %d\n",eventId);
      event_delete(eventId);
    } else {
      fprintf(stderr,"Warning eventDelete event not found (%d)\n",eventId);
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
        channelType = CTYPE_NONE;
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
        if (!typestr) {
          channelType = CTYPE_UNKNOWN;
        } else {
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
            fprintf(stderr,"Type=%s (unknown)\n",typestr);
            channelType = CTYPE_UNKNOWN;
          }
          free(typestr);
        }
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
  } else if (strcmp(method,"dvrEntryAdd")==0) {
    /* TODO */
  } else {
    fprintf(stderr,"%s: Received message %s\n",debugtext,method);
    //htsp_dump_message(msg);
  }
}

static void do_pause(struct codecs_t* codecs, int pause)
{
  if (pause) {
    if (!codecs->is_paused) {
      /* Currently playing, pause */
      fprintf(stderr,"[PAUSE]\n");
      codec_pause(&codecs->acodec);
      codec_pause(&codecs->vcodec);
      codecs->is_paused = 1;
    }
  } else {
    if (codecs->is_paused) {
      /* Currently paused, resume playback */
      fprintf(stderr,"[RESUME]\n");
      codec_resume(&codecs->acodec);
      codec_resume(&codecs->vcodec);
      codecs->is_paused = 0;
    }
  }
}


/* The HTSP thread reads from the network and passes the incoming stream packets to the
   appropriate codec (video/audio/subtitle) */
void* htsp_receiver_thread(struct codecs_t* codecs)
{
  struct htsp_message_t msg;
  int res;
  struct packet_t* packet;
  int ok = 1;
  int current_subscriptionId = -1;
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

  while (ok) {
    htsp_lock(&htsp);
    res = htsp_recv_message(&htsp,&msg,100);
    current_subscriptionId = htsp.subscriptionId;
    htsp_unlock(&htsp);
    if (res == 1) {
      usleep(100000);
    }

    if (res == 0) {
      int free_msg = 1;  // We want to free this message, unless this flag is set to zero

      char* method = htsp_get_string(&msg,"method");
      //fprintf(stderr,"method=%s\n",method);

      if (method != NULL) {
        if (strcmp(method,"subscriptionStart")==0) {
          if (htsp_parse_subscriptionStart(&msg,&codecs->subscription) > 0) {
            fprintf(stderr,"FATAL ERROR: Cannot parse subscriptionStart\n");
            exit(1);
          }

          if (codecs->subscription.videostream >= 0) {
            fprintf(stderr,"[htsp_receiver_thread] - creating video codec\n");
            if (codecs->subscription.streams[codecs->subscription.videostream].codec == HMF_VIDEO_CODEC_MPEG2) {
              if (hw_mpeg2) {
                vcodec_omx_init(&codecs->vcodec,OMX_VIDEO_CodingMPEG2);
              } else {
                vcodec_mpeg2_init(&codecs->vcodec);
              }
            } else {
              vcodec_omx_init(&codecs->vcodec,OMX_VIDEO_CodingAVC);
            }
            codecs->vcodec.acodec = &codecs->acodec;
          }

          if (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_MPEG) {
            acodec_mpeg_init(&codecs->acodec);
            DEBUGF("Initialised mpeg codec\n");
          } else if (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AAC) {
            acodec_aac_init(&codecs->acodec);
            DEBUGF("Initialised AAC codec\n");
          } else if (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AC3) {
            acodec_a52_init(&codecs->acodec);
            DEBUGF("Initialised A/52 codec\n");
          }

          // TODO: Subtitle thread

        } else if (strcmp(method,"subscriptionStatus")==0) {
          char* status = htsp_get_string(&msg,"status");

          if ((status != NULL) && (strcmp(status,"OK")!=0)) {
            fprintf(stderr,"subscriptionStatus: %s\n",status);
            free(status);
          }
        } else if (strcmp(method,"muxpkt")==0) {
          int stream;
          htsp_get_int(&msg,"stream",&stream);
          int subscriptionId;
          htsp_get_int(&msg,"subscriptionId",&subscriptionId);

          if (subscriptionId == current_subscriptionId) {
            if (stream==codecs->subscription.streams[codecs->subscription.videostream].index) {
              packet = malloc(sizeof(*packet));
              packet->buf = msg.msg;

              int frametype;
              htsp_get_int(&msg,"frametype",&frametype);

              // htsp_dump_message(&msg);
              if (htsp_get_int64(&msg,"pts",&packet->PTS) > 0) {
                fprintf(stderr,"ERROR: No PTS in video packet, dropping\n");
              } else {
                if (htsp_get_int64(&msg,"dts",&packet->DTS) > 0) {
                  fprintf(stderr,"ERROR: No DTS in video packet, dropping\n");
                } else {
                  htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

#ifdef DUMP_VIDEO
                  write(fd,packet->packet,packet->packetlength);
#endif
                  codec_queue_add_item(&codecs->vcodec,packet);

                  free_msg = 0;   // Don't free this message
                }
              }
            } else if ((codecs->acodec.is_running) && (stream==codecs->subscription.streams[codecs->subscription.audiostream].index) &&
                       ((codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_MPEG) ||
                        (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AC3) ||
                        (codecs->subscription.streams[codecs->subscription.audiostream].codec == HMF_AUDIO_CODEC_AAC))
                      ) {
              packet = malloc(sizeof(*packet));
              packet->buf = msg.msg;
              if (htsp_get_int64(&msg,"pts",&packet->PTS) > 0) {
                fprintf(stderr,"ERROR: No PTS in audio packet, dropping\n");
              } else {
                htsp_get_bin(&msg,"payload",&packet->packet,&packet->packetlength);

                codec_queue_add_item(&codecs->acodec,packet);
                free_msg = 0;   // Don't free this message
              }
            }
          }
        } else if (strcmp(method,"subscriptionSkip")==0) {
          codec_flush_queue(&codecs->vcodec);
          codec_flush_queue(&codecs->acodec);
          do_pause(codecs,0);
        } else {
          process_message(method,&msg,"htsp_receiver_thread");
        }
        free(method);
      }

      if (free_msg)
        htsp_destroy_message(&msg);
    }
  }

#ifdef DUMP_VIDEO
  close(fd);
#endif

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

#define KEY_RELEASE 0
#define KEY_PRESS 1
#define KEY_KEEPING_PRESSED 2

int get_input_key(int fd)
{
  struct input_event ev[64];
  int i;

  size_t rb = read(fd, ev, sizeof(ev));

  if (rb < (int) sizeof(struct input_event)) {
    fprintf(stderr,"Short read\n");
    return -1;
  }

  for (i = 0; i < (int)(rb / sizeof(struct input_event));i++) {
    if (ev[i].type == EV_KEY) {
      if ((ev[i].value == KEY_PRESS) || (ev[i].value == KEY_KEEPING_PRESSED)) {
        fprintf(stderr,"input code %d\n",ev[1].code);
        switch(ev[1].code) {
          case KEY_0: return '0';
          case KEY_1: return '1';
          case KEY_2: return '2';
          case KEY_3: return '3';
          case KEY_4: return '4';
          case KEY_5: return '5';
          case KEY_6: return '6';
          case KEY_7: return '7';
          case KEY_8: return '8';
          case KEY_9: return '9';
          case KEY_H: return 'h';
          case KEY_I: return 'i';
          case KEY_Q: return 'q';
          case KEY_N: return 'n';
          case KEY_P: return 'p';
          case KEY_PAGEUP: return 'n';
          case KEY_PAGEDOWN: return 'p';
          case KEY_UP: return 'u';
          case KEY_DOWN: return 'd';
          case KEY_LEFT: return 'l';
          case KEY_RIGHT: return 'r';
    
          default: break;
        }
      }
    }
  }

  return -1;
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
    int inputfd;
    char inputname[256] = "Unknown";
    char *inputdevice = "/dev/input/event0";

    htsp.host = NULL;
    htsp.ip = NULL;

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

#ifdef ENABLE_CEC
    cec_init(0);
#endif

    hw_mpeg2 = mpeg2_codec_enabled();

    if (hw_mpeg2) {
      fprintf(stderr,"Using hardware MPEG-2 decoding\n");
    } else {
      fprintf(stderr,"Using software MPEG-2 decoding\n");
    }

    if ((inputfd = open(inputdevice, O_RDONLY)) >= 0) {
      ioctl (inputfd, EVIOCGNAME (sizeof (inputname)), inputname);
      fprintf(stderr,"Using %s - %s\n", inputdevice, inputname);

      /* Disable auto-repeat (for now...) */
      int ioctl_params[2] = { 0, 0 };
      ioctl(inputfd,EVIOCSREP,ioctl_params);
    }

    osd_init(&osd);

    htsp_init(&htsp);

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
    res = htsp_recv_message(&htsp,&msg,0);

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
       res = htsp_recv_message(&htsp,&msg,0);

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

    /* We have finished the initial connection and sync, now start the
       receiving thread */
    pthread_create(&htspthread,NULL,(void * (*)(void *))htsp_receiver_thread,(void*)&codecs);

    user_channel_id = channels_getid(channel);
    if (user_channel_id < 0)
      user_channel_id = channels_getfirst();

    actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);

    memset(&codecs.vcodec,0,sizeof(codecs.vcodec));
    memset(&codecs.acodec,0,sizeof(codecs.acodec));
    codecs.is_paused = 0;

next_channel:
    osd_blank_video(&osd,0); /* Don't blank the screen for now - leave the transition visbible for debugging */
    double blank_video_timeout = get_time() + 1000;

    fprintf(stderr,"lock0\n");
    if (codecs.vcodec.thread) {
      codec_stop(&codecs.vcodec);
      pthread_join(codecs.vcodec.thread,NULL);
      fprintf(stderr,"[main thread] - killed video thread\n");
    }

    fprintf(stderr,"lock1\n");
    if (codecs.acodec.thread) {
      codec_stop(&codecs.acodec);
      pthread_join(codecs.acodec.thread,NULL);
      fprintf(stderr,"[main thread] - killed audio thread\n");
    }
    fprintf(stderr,"lock3\n");

    fprintf(stderr,"lock4\n");
    htsp_lock(&htsp);
    fprintf(stderr,"lock5\n");

    if (htsp.subscriptionId > 0) {
      res = htsp_create_message(&msg,HMF_STR,"method","unsubscribe",HMF_S64,"subscriptionId",htsp.subscriptionId,HMF_NULL);
      res = htsp_send_message(&htsp,&msg);
      htsp_destroy_message(&msg);
    }

    memset(&codecs.acodec,0,sizeof(codecs.acodec));
    
    fprintf(stderr,"lock6\n");
    htsp_unlock(&htsp);

    fprintf(stderr,"Tuning to channel %d - \"%s\"\n",channels_getlcn(user_channel_id),channels_getname(user_channel_id));

    osd_show_info(&osd,user_channel_id);
    osd_cleartime = get_time() + 5000;

    fprintf(stderr,"Waiting for lock\n");
    htsp_lock(&htsp);
    fprintf(stderr,"locked\n");
    res = htsp_create_message(&msg,HMF_STR,"method","subscribe",
                                   HMF_S64,"channelId",actual_channel_id,
                                   HMF_S64,"timeshiftPeriod",3600,
                                   HMF_S64,"normts",1,
                                   HMF_S64,"subscriptionId",++htsp.subscriptionId,
                                   HMF_NULL);
    res = htsp_send_message(&htsp,&msg);
    htsp_unlock(&htsp);

    htsp_destroy_message(&msg);
    fprintf(stderr,"HERE - subscribe message sent\n");

    /* UI loop */

    int new_channel;
    double new_channel_timeout;

    new_channel = -1;
    new_channel_timeout = 0;
    while (1) {
      int c;
      struct timeval tv = { 0L, 100000L };  /* 100ms */
      fd_set fds;
      int maxfd = 0;
      FD_ZERO(&fds);
      FD_SET(0, &fds);
      if (inputfd >= 0) {
        FD_SET(inputfd,&fds);
        maxfd = inputfd;
      }
      c = -1;
      if (select(maxfd+1, &fds, NULL, NULL, &tv)==0) {
        c = -1;
      } else {
        if (FD_ISSET(0,&fds)) {
          c = getchar();
        }
        if ((inputfd >= 0) && (FD_ISSET(inputfd,&fds))) {
          c = get_input_key(inputfd);
        }
      }

#ifdef ENABLE_CEC
      if (c==-1) {
        c = cec_get_keypress();
      }
#endif

      if (c != -1) {
        DEBUGF("char read: 0x%08x ('%c')\n", c,(isalnum(c) ? c : ' '));

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
            if (osd_cleartime) {
              /* Hide info if currently shown */
              osd_clear(&osd);
              osd_cleartime = 0;
	    } else {
              osd_show_info(&osd,user_channel_id);
              osd_cleartime = get_time() + 20000; /* 20 second timeout */
            }

            break;

          case 'c':
            channels_dump();
            break;

          case 'h':
            auto_hdtv = 1 - auto_hdtv;
            int new_actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
            if (new_actual_channel_id != actual_channel_id) {
              actual_channel_id = new_actual_channel_id;
              goto next_channel;
            }
            break;

          case ' ':
            do_pause(&codecs,1-codecs.is_paused);

            htsp_lock(&htsp);
            res = htsp_create_message(&msg,HMF_STR,"method","subscriptionSpeed",
				           HMF_S64,"speed",(codecs.is_paused == 0 ? 100 : 0),
                                           HMF_S64,"subscriptionId",htsp.subscriptionId,
                                           HMF_NULL);
            res = htsp_send_message(&htsp,&msg);
            htsp_unlock(&htsp);
            htsp_destroy_message(&msg);
            break;

          case 'n':
          case 'p':
            if (c=='n') user_channel_id = channels_getnext(user_channel_id);
            else user_channel_id = channels_getprev(user_channel_id);
            actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);

            goto next_channel;

          case 'u':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,10*60);  // +10 minutes
            break;

          case 'd':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,-10*60); // -10 minutes
            break;

          case 'l':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,-30);    // -30 seconds
            break;

          case 'r':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,30);     // +30 seconds
            break;

            break;

          default:
            break;            
        }
      }

      if ((osd_cleartime) && (get_time() > osd_cleartime)) {
        osd_clear(&osd);
        osd_cleartime = 0;
      }

      if ((blank_video_timeout) && (get_time() > blank_video_timeout)) {
        osd_blank_video(&osd,0);
        blank_video_timeout = 0;
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
