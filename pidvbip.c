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
#include <bcm_host.h>

#include "configfile.h"
#include "vcodec_omx.h"
#include "acodec_omx.h"
#include "sha1.h"
#include "htsp.h"
#include "channels.h"
#include "events.h"
#include "debug.h"
#include "osd.h"
#include "avahi.h"
#include "cec.h"
#include "msgqueue.h"
#include "avplay.h"
#include "omx_utils.h"
#include "input.h"

struct omx_pipeline_t omxpipe;

/* TODO: Should this be global? */
struct htsp_t htsp;

static struct termios orig;

/* Messages to the HTSP receiver thread - low eight bits are a parameter */
#define MSG_CHANGE_AUDIO_STREAM 0x100

void reset_stdin(void)
{
  tcsetattr(0, TCSANOW, &orig);
}

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

static void process_message(char* method,struct htsp_message_t* msg,char* debugtext)
{
  if ((strcmp(method,"eventAdd")==0) || (strcmp(method,"eventUpdate")==0)) {
    process_event_message(method,msg);
  } else if (strcmp(method,"eventDelete")==0) {
    uint32_t eventId;
    if (htsp_get_uint(msg,"eventId",&eventId)==0) {
      //fprintf(stderr,"eventDelete: %d\n",eventId);
      event_delete(eventId, msg->server);
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
        channels_add(msg->server, channelNumber,channelId,channelName,channelType,eventid,nexteventid);
        //fprintf(stderr,"channelAdd - id=%d,lcn=%d,name=%s,current_event=%d,next_event=%d\n",channelId,channelNumber,channelName,eventid,nexteventid);
      } else {
        channels_update(msg->server, channelNumber,channelId,channelName,channelType,eventid,nexteventid);
        //fprintf(stderr,"channelUpdate - id=%d,current_event=%d,next_event=%d\n",channelId,eventid,nexteventid);
      }
    }
  } else if (strcmp(method,"queueStatus")== 0) {
    /* Are we interested? */
  } else if (strcmp(method,"signalStatus")== 0) {
    /* Are we interested? */
  } else if (strcmp(method,"timeshiftStatus")==0) {
    /* TODO */
  } else if (strcmp(method,"tagAdd")==0) {
    /* TODO */
  } else if (strcmp(method,"tagUpdate")==0) {
    /* TODO */
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


static struct msgqueue_t htsp_msgqueue;

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
    int x = msgqueue_get(&htsp_msgqueue,0);
    if (x != -1) {
      switch (x & 0xffffff00) {
        case MSG_CHANGE_AUDIO_STREAM:
          codec_new_channel(&codecs->acodec);
          codecs->subscription.audiostream = x & 0xff;
          codecs->acodec.acodectype = codecs->subscription.streams[codecs->subscription.audiostream].codec;
          codecs->acodec.first_packet = 1;
          codecs->acodec.is_running = 1;
          fprintf(stderr,"Processed audio stream change - new stream is %d\n",x&0xff);
          break;

        default:
          fprintf(stderr,"Unknown HTSP thread message 0x%08x\n",x);
      }
    }

    htsp_lock(&htsp);
    res = htsp_recv_message(&htsp,-1,&msg,100);
    //fprintf(stderr,"Received messaged from server %d\n",msg.server);
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

          if (codecs->subscription.streams[codecs->subscription.videostream].codec == HMF_VIDEO_CODEC_MPEG2) {
            codecs->vcodec.vcodectype = OMX_VIDEO_CodingMPEG2;
          } else {
            codecs->vcodec.vcodectype = OMX_VIDEO_CodingAVC;
          }
          codecs->vcodec.width = codecs->subscription.streams[codecs->subscription.videostream].width;
          codecs->vcodec.height = codecs->subscription.streams[codecs->subscription.videostream].height;

          codecs->acodec.acodectype = codecs->subscription.streams[codecs->subscription.audiostream].codec;

          codec_new_channel(&codecs->vcodec);
          codec_new_channel(&codecs->acodec);

          /* Resume sending packets to codecs */
          codecs->vcodec.is_running = 1;
          codecs->acodec.is_running = 1;

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
	    //            fprintf(stderr,"muxpkt: stream=%d, audio_stream=%d, video_stream=%d\n",stream,codecs->subscription.streams[codecs->subscription.audiostream].index,codecs->subscription.streams[codecs->subscription.videostream].index);

            if (stream==codecs->subscription.streams[codecs->subscription.videostream].index) {
              packet = malloc(sizeof(*packet));
              packet->buf = msg.msg;
              htsp_get_int(&msg,"frametype",&packet->frametype);

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
		//fprintf(stderr,"Queuing acodec packet - PTS=%lld, size=%d\n",packet->PTS, packet->packetlength);
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

void usage(void)
{
  fprintf(stderr,"pidvbip - tvheadend client for the Raspberry Pi\n");
  fprintf(stderr,"\nOptions:\n\n");
}

static int get_actual_channel(int auto_hdtv, int user_channel_id)
{
  int actual_channel_id = user_channel_id;
  int server;

  if ((auto_hdtv) && (channels_gettype(user_channel_id)==CTYPE_SDTV)) {
    uint32_t current_eventId;
    channels_geteventid(user_channel_id, &current_eventId, &server);
    int hd_channel = event_find_hd_version(current_eventId, server);
    if (hd_channel >= 0) {
      fprintf(stderr,"Auto-switching to channel %d (%s)\n",channels_getlcn(hd_channel),channels_getname(hd_channel));
      actual_channel_id = hd_channel;
    }
  }

  return actual_channel_id;
}

extern struct configfile_parameters global_settings;

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
    int curr_streaming = 1;
    struct msgqueue_t msgqueue;
    int zoom = 0;
    int i;
 
    for (i=0;i<MAX_HTSP_SERVERS;i++) {
      htsp.host[i] = NULL;
      htsp.ip[i] = NULL;
    }

    msgqueue_init(&msgqueue);

    memset(&omxpipe,0,sizeof(omxpipe));
    pthread_mutex_init(&omxpipe.omx_active_mutex, NULL);
    pthread_cond_init(&omxpipe.omx_active_cv, NULL);

    parse_args(argc,argv);
    dump_settings();

    if ((strcmp(global_settings.audio_dest,"hdmi")) && (strcmp(global_settings.audio_dest,"local"))) {
      fprintf(stderr,"Defaulting audio_dest to hdmi\n");
      global_settings.audio_dest = "hdmi";
    }

    // Still no value for htsp.host htsp.port so try avahi
#if ENABLE_AVAHI
    if (global_settings.avahi && ((!global_settings.host || global_settings.host[0] == 0 || global_settings.port == 0))) {
      avahi_discover_tvh(&htsp);
    } else 
#endif
    {
      fprintf(stderr,"Copying hosts - host2=%s\n",global_settings.host2);
      htsp.host[0] = global_settings.host;
      htsp.port[0] = global_settings.port;
      htsp.host[1] = global_settings.host2;
      htsp.port[1] = global_settings.port2;
    };

    if ((htsp.host[0] == NULL) || (htsp.host[0][0] == 0 || htsp.port[0] == 0)) {
      fprintf(stderr,"ERROR: Could not obtain host or port for TVHeadend\n");
      fprintf(stderr,"       Please ensure config file or cmd-line params\n");
      fprintf(stderr,"       are provided and try again\n");
      usage();
      exit(1);
    };

    htsp.numservers = 1;
    fprintf(stderr,"Using host \"%s:%d\"\n",htsp.host[0],htsp.port[0]);

    fprintf(stderr,"host[1]=%s\n",htsp.host[1]);
    if (htsp.host[1]) {
      htsp.numservers++;
      fprintf(stderr,"Using host \"%s:%d\"\n",htsp.host[1],htsp.port[1]);
    }
    bcm_host_init();

    OERR(OMX_Init());

#if ENABLE_LIBCEC
    if (!global_settings.nocec) {
      cec_init(0,&msgqueue);
    }
#endif

    if (! mpeg2_codec_enabled()) {
      fprintf(stderr,"WARNING: No hardware MPEG-2 license detected - MPEG-2 video will not work\n");
    }

    channels_init();
    events_init();

    struct termios new;
    tcgetattr(0, &orig);
    memcpy(&new, &orig, sizeof(struct termios));
    new.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    new.c_cc[VTIME] = 0;
    new.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &new);
    atexit(reset_stdin);

    input_init(&msgqueue);

    osd_init(&osd);

    osd_alert(&osd, "Connecting to server...");

    htsp_init(&htsp);

    fprintf(stderr,"Connecting to %d servers\n",htsp.numservers);
    for (i=0;i<htsp.numservers;i++) {
      fprintf(stderr,"Connecting to server %d - %s\n",i,htsp.host[i]);
      if ((res = htsp_connect(&htsp, i)) > 0) {
        fprintf(stderr,"Error connecting to htsp server %d, aborting.\n", i);
        return 2;
      }

      res = htsp_login(&htsp, i, global_settings.username, global_settings.password);

      if (res > 0) {
        fprintf(stderr,"Could not login to server %d\n", i);
        return 3;
      }
      osd_alert(&osd, "Loading channels...");
      res = htsp_create_message(&msg,HMF_STR,"method","enableAsyncMetadata",HMF_S64,"epg",1,HMF_NULL);
      res = htsp_send_message(&htsp,i, &msg);
      htsp_destroy_message(&msg);

      // Receive the acknowledgement from enableAsyncMetadata
      res = htsp_recv_message(&htsp,i, &msg,0);

      int num_events = 0;
      int done = 0;
      while (!done)
      {
        // TODO: Store all received channels/tags/dvr data and stop when completed message received.
        res = htsp_recv_message(&htsp,i, &msg,0);

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

      osd_alert(&osd, NULL);

      fprintf(stderr,"Initial sync completed - read data for %d events\n",num_events);
    }

    if (channels_getcount() == 0) {
      fprintf(stderr,"No channels available, exiting.\n");
      exit(1);
    }

    /* Initial channel choice */
    fprintf(stderr,"Startup stopped %d\n", global_settings.startup_stopped);
    if(!global_settings.startup_stopped) {
      if (global_settings.initial_channel)
          channel = global_settings.initial_channel;
      user_channel_id = channels_getid(channel);
      if (user_channel_id < 0) {
        fprintf(stderr," Channels_getfirst\n");
        user_channel_id = channels_getfirst();
      };
      fprintf(stderr,"Channel %d\n",user_channel_id);
      actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
    };

    /* We have finished the initial connection and sync, now start the
       receiving thread */
    msgqueue_init(&htsp_msgqueue);
    pthread_create(&htspthread,NULL,(void * (*)(void *))htsp_receiver_thread,(void*)&codecs);

    memset(&codecs.vcodec,0,sizeof(codecs.vcodec));
    memset(&codecs.acodec,0,sizeof(codecs.acodec));
    
    codecs.is_paused = 0;

    codecs.vcodec.acodec = &codecs.acodec;
    vcodec_omx_init(&codecs.vcodec, &omxpipe, global_settings.audio_dest);
    acodec_omx_init(&codecs.acodec, &omxpipe);

#if ENABLE_LIBAVFORMAT
    if (global_settings.avplay) {
      /* Temporary hack to test avplay() */
      avplay(&codecs, global_settings.avplay);
    }
#endif

next_channel:
    osd_blank_video(&osd,0); /* Don't blank the screen for now - leave the transition visbible for debugging */
    double blank_video_timeout = get_time() + 1000;

    htsp_lock(&htsp);

    if (htsp.subscriptionId > 0) {
      res = htsp_create_message(&msg,HMF_STR,"method","unsubscribe",HMF_S64,"subscriptionId",htsp.subscriptionId,HMF_NULL);
      res = htsp_send_message(&htsp,htsp.subscriptionServer,&msg);
      htsp_destroy_message(&msg);
    }

    htsp_unlock(&htsp);

    fprintf(stderr,"Tuning to channel %d - \"%s\"\n",channels_getlcn(user_channel_id),channels_getname(user_channel_id));

    osd_show_info(&osd,user_channel_id,5000);

    int tvh_id;
    channels_gettvhid(actual_channel_id,&tvh_id,&htsp.subscriptionServer);
    fprintf(stderr,"Waiting for lock\n");
    htsp_lock(&htsp);
    fprintf(stderr,"locked\n");
    res = htsp_create_message(&msg,HMF_STR,"method","subscribe",
                                   HMF_S64,"channelId",tvh_id,
                                   HMF_S64,"timeshiftPeriod",3600,
                                   HMF_S64,"normts",1,
                                   HMF_S64,"subscriptionId",++htsp.subscriptionId,
#if 0
                                   /* Transcoding */
                                   HMF_STR,"videoCodec","H264",
                                   HMF_STR,"audioCodec","MPEG2AUDIO",
                                   HMF_S64,"maxResolution",576,  /* Max height of the stream */
#endif
                                   HMF_NULL);
    res = htsp_send_message(&htsp,htsp.subscriptionServer,&msg);
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

      c = msgqueue_get(&msgqueue, 100);

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
            osd_show_newchannel(&osd,new_channel);
            new_channel_timeout = get_time() + 1000;
            break;

          case 'q':
            goto done;

          case 'i': /* Toggle info screen */
            if (osd.osd_state == OSD_INFO) {
              /* Hide info if currently shown */
              osd_clear(&osd);
	    } else {
              osd_show_info(&osd,user_channel_id, 60000); /* 60 second timeout */
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
              goto change_channel;
            }
            break;

          case ' ':
            do_pause(&codecs,1-codecs.is_paused);

            htsp_lock(&htsp);
            res = htsp_create_message(&msg,HMF_STR,"method","subscriptionSpeed",
				           HMF_S64,"speed",(codecs.is_paused == 0 ? 100 : 0),
                                           HMF_S64,"subscriptionId",htsp.subscriptionId,
                                           HMF_NULL);
            res = htsp_send_message(&htsp,htsp.subscriptionServer,&msg);
            htsp_unlock(&htsp);
            htsp_destroy_message(&msg);
            break;

          case 'n':
          case 'p':
            if (c=='n') user_channel_id = channels_getnext(user_channel_id);
            else user_channel_id = channels_getprev(user_channel_id);
            actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);

            goto change_channel;

          case 'u':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,htsp.subscriptionServer,10*60);  // +10 minutes
            break;

          case 'd':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,htsp.subscriptionServer,-10*60); // -10 minutes
            break;

          case 'l':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,htsp.subscriptionServer,-30);    // -30 seconds
            break;

          case 'r':
            do_pause(&codecs,1);
            htsp_send_skip(&htsp,htsp.subscriptionServer,30);     // +30 seconds
            break;

          case 'o':
            // Toggle stop/start streaming channel
            if (curr_streaming == 1) {
              htsp_lock(&htsp);
	      if (htsp.subscriptionId > 0) {
                osd_alert(&osd, "Stopping current subscription");
	        res = htsp_create_message(&msg,HMF_STR,"method","unsubscribe",HMF_S64,"subscriptionId",htsp.subscriptionId,HMF_NULL);
	        res = htsp_send_message(&htsp,htsp.subscriptionServer,&msg);
	        htsp_destroy_message(&msg);
                osd_blank_video(&osd,0);
              };
              htsp_unlock(&htsp);
              curr_streaming = 0;
	    } else {
              osd_alert(&osd, "Restarting subscription");
              curr_streaming = 1;
              actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
              goto change_channel;
            };
            break;

          case 'a':
            {}; /* Keep gcc happy */
            int i;
            int first_audio_stream = -1;
            int next_audio_stream = -1;
            for (i=0;i<codecs.subscription.numstreams;i++) {
              struct htsp_stream_t *stream = &codecs.subscription.streams[i];
              if (stream->type == HMF_STREAM_AUDIO) {
                if (first_audio_stream == -1)
                  first_audio_stream = i;

                if ((next_audio_stream == -1) && (i>codecs.subscription.audiostream))
                  next_audio_stream = i;
              }
            }

            if (next_audio_stream == -1)
              next_audio_stream = first_audio_stream;

            if (codecs.subscription.audiostream != next_audio_stream) {
	      msgqueue_add(&htsp_msgqueue,MSG_CHANGE_AUDIO_STREAM | next_audio_stream);
            }

            osd_show_audio_menu(&osd,&codecs,next_audio_stream);

            break;

          case 'z':
            zoom = 1 - zoom;
            codec_send_message(&codecs.vcodec,MSG_ZOOM,(void*)zoom);
            break;

          default:
            break;            
        }
      }

      osd_update(&osd, user_channel_id);

      if ((new_channel_timeout) && (get_time() >= new_channel_timeout)) {
        fprintf(stderr,"new_channel = %d\n",new_channel);
        int new_channel_id = channels_getid(new_channel);
        if (new_channel_id >= 0) {
          user_channel_id = new_channel_id;
          actual_channel_id = get_actual_channel(auto_hdtv,user_channel_id);
          goto change_channel;
        } else {
          osd_clear_newchannel(&osd);
          fprintf(stderr,"No such channel\n");
          new_channel = -1;
          new_channel_timeout = 0;
        }
      }
    }

change_channel:
    goto next_channel;

done:
    OERR(OMX_Deinit());
    return 0;
}
