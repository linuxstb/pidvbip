#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "htsp.h"
#include "debug.h"

static int create_tcp_socket()
{
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  return sock;
}

static char *get_ip(char *host)
{
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}

int htsp_connect(struct htsp_t* htsp, char* host, int port)
{
    int res;

    htsp->host = host;
    htsp->port = port;

    htsp->sock = create_tcp_socket();
    htsp->ip = get_ip(host);

    fprintf(stderr,"Connecting to %s (%s) port %d...\n",htsp->host,htsp->ip,htsp->port);    

    htsp->remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
    htsp->remote->sin_family = AF_INET;

    res = inet_pton(AF_INET, htsp->ip, (void *)(&(htsp->remote->sin_addr.s_addr)));

    if (res < 0) {
        perror("Can't set remote->sin_addr.s_addr");
        exit(1);
    } else if (res == 0) {
        fprintf(stderr, "%s is not a valid IP address\n", htsp->ip);
        return 1;
    }
    htsp->remote->sin_port = htons(port);
 
    if (connect(htsp->sock, (struct sockaddr *)htsp->remote, sizeof(struct sockaddr)) < 0){
        perror("Could not connect");
        return 2;
    }

    return 0;
}

static char* hmf_labels[] = { "", "MAP","S64","STR","BIN","LIST","DBL" };

static int get_uint32_be(unsigned char* buf)
{
   return((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

static void put_uint32_be(unsigned char* buf, uint32_t x)
{
   buf[0] = (x >> 24) & 0xff;
   buf[1] = (x >> 16) & 0xff;
   buf[2] = (x >>  8) & 0xff;
   buf[3] = x & 0xff;
}

void htsp_dump_binary(unsigned char* buf, int len)
{
    int i;
    uint64_t x = 0;

    while (len > 0) {
       int type = buf[0]; if (type > 6) { type = 0; }
       int namelength = buf[1];
       int datalength = get_uint32_be(buf + 2);
       buf += 6; len -= 6;

       DEBUGF("type=%d, datalen=%d %s: ",type,datalength,hmf_labels[type]);

       for (i=0;i<namelength;i++) { DEBUGF(stderr,"%c",buf[i]); }
       buf += namelength; len -= namelength;

       fprintf(stderr,"=");
       switch (type) {
           case HMF_STR:
             DEBUGF("\"");
             for (i=0;i<datalength;i++) { DEBUGF("%c",buf[i]); }
             DEBUGF("\"");
             break;

           case HMF_BIN:
             DEBUGF("\"");
             //for (i=0;i<datalength;i++) { DEBUGF(" 0x%02x",buf[i]); }
             DEBUGF("\"");
             break;

           case HMF_S64:
             x = 0;
             for (i=datalength-1;i>=0;i--) {
                x <<= 8;
                x |= buf[i];
             }
             DEBUGF("%lld",x);
             DEBUGF(" [");
             for (i=0;i<datalength;i++) { DEBUGF(" 0x%02x",buf[i]); }
             DEBUGF(" ]");
             break;

           case HMF_LIST:
           case HMF_MAP:
             DEBUGF("\n");
	     htsp_dump_binary(buf, datalength);
             break;

           default:
             break;
       }
       DEBUGF("\n");
       buf += datalength; len -= datalength;
    }

}

void htsp_dump_message(struct htsp_message_t* msg)
{
  htsp_dump_binary(msg->msg + 4, msg->msglen - 4);
}


void htsp_destroy_message(struct htsp_message_t* msg)
{
  free(msg->msg);
}

int htsp_create_message(struct htsp_message_t* msg, ...)
{
  va_list argp;
  int type;
  char* fieldname;
  int fieldnamelen;
  int datalen;
  char* s;
  int i,ii;
  unsigned char* d;
  unsigned char* p;
  
  msg->msglen = 4;

  va_start(argp,msg);

  type = va_arg(argp, int);
  while (type != HMF_NULL) {
    msg->msglen++;

    fieldname = va_arg(argp,char*);
    msg->msglen += strlen(fieldname) + 1 + 4;

    switch(type) {
      case HMF_STR:
         s = va_arg(argp,char*);
         msg->msglen += strlen(s);
         break;

      case HMF_S64:
         i = va_arg(argp,int);
         while (i > 0) {
           msg->msglen++;
           i >>= 8;
         }
         break;

      case HMF_BIN:
         i = va_arg(argp,int);
         va_arg(argp,unsigned char*);
         msg->msglen += i;
         break;

      default:
        fprintf(stderr,"FATAL ERROR: Unsupported type in htsp_create_message (%d)\n",type);
        exit(1);
        break;
    }

    type = va_arg(argp, int);
  }

  va_end(argp);

  msg->msg = malloc(msg->msglen);

  p = msg->msg;

  put_uint32_be(p,msg->msglen - 4); p += 4;
  va_start(argp,msg);

  type = va_arg(argp, int);
  while (type != HMF_NULL) {
    *p++ = type;

    fieldname = va_arg(argp,char*);
    fieldnamelen = strlen(fieldname);

    *p++ = fieldnamelen;
    
    switch(type) {
      case HMF_STR:
         s = va_arg(argp,char*);
         datalen = strlen(s);
         put_uint32_be(p,datalen); p += 4;
         memcpy(p,fieldname,fieldnamelen); p += fieldnamelen;
         memcpy(p,s,datalen); p += datalen;
         break;

      case HMF_S64:
         i = va_arg(argp,int);
         ii = i;
         datalen = 0;
         while (ii > 0) {
           datalen++;
           ii >>= 8;
         }
         put_uint32_be(p,datalen); p += 4;
         memcpy(p,fieldname,fieldnamelen); p += fieldnamelen;

         while (i != 0) {
           *p++ = (i & 0xff);
           i >>= 8;
         }
         break;

      case HMF_BIN:
         datalen = va_arg(argp,int);
         d = va_arg(argp,unsigned char*);
         put_uint32_be(p,datalen); p += 4;
         memcpy(p,fieldname,fieldnamelen); p += fieldnamelen;
         memcpy(p,d,datalen); p += datalen;
         break;

      default:
        fprintf(stderr,"FATAL ERROR: Unsupported type in htsp_create_message (%d)\n",type);
        exit(1);
        break;
    }

    type = va_arg(argp, int);
  }

  va_end(argp);

  return 0;
}

int htsp_send_message(struct htsp_t* htsp, struct htsp_message_t* msg)
{
  int res;
  int sent = 0;

  while(sent < msg->msglen)
  {
    res = send(htsp->sock, msg->msg + sent, msg->msglen-sent, 0);
    if(res == -1){
      perror("Can't send query");
      return 1;
    }
    sent += res;
  }

  return 0;
}

int htsp_recv_message(struct htsp_t* htsp, struct htsp_message_t* msg)
{
  int res;
  unsigned char buf[4];

  //fprintf(stderr,"Waiting for response...\n");
  res = recv(htsp->sock, buf, 4, 0);

  if (res < 0) {
     fprintf(stderr,"Error in recv\n");
     return 1;
  }

  msg->msglen = get_uint32_be(buf);

  msg->msg = malloc(msg->msglen + 4);
  if (msg->msg == NULL) {
    fprintf(stderr,"FATAL ERROR - out of memory (tried to allocate %d bytes)\n",msg->msglen + 4);
    exit(1);
  }
  memcpy(msg->msg, buf, 4);

  int bytesleft = msg->msglen;
  unsigned char* p = msg->msg + 4;
  while (bytesleft) {
    res = recv(htsp->sock, p, bytesleft, 0);

    if (res < 0) {
      fprintf(stderr,"Error in recv\n");
      return 1;
    }

    p += res;
    bytesleft -= res;
  }

  return 0;
}

int htsp_login(struct htsp_t* htsp)
{
  struct htsp_message_t msg;
  int res;

  htsp_create_message(&msg,HMF_STR,"method","hello",
                           HMF_STR,"clientname","htsptest",
                           HMF_S64,"htspversion",1,
                           HMF_S64,"seq",1,
                           HMF_NULL);

  //fprintf(stderr,"Sending hello message - %d bytes\n",msg.msglen);

  if ((res = htsp_send_message(htsp,&msg)) > 0) {
    fprintf(stderr,"Could not send message\n");
    return 1;
  }

  htsp_destroy_message(&msg);

  res = htsp_recv_message(htsp,&msg);

  if (res == 0) {
    htsp_dump_message(&msg);
  }

  htsp_destroy_message(&msg);

  return 0;
}

char* htsp_get_string(struct htsp_message_t* msg, char* name)
{
  unsigned char* buf = msg->msg;
  int len = msg->msglen;
  int msglen = get_uint32_be(buf); buf += 4; len -= 4;
  int matchlen = strlen(name);
  char* s;

  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    if ((type == HMF_STR) && (namelength==matchlen) && (memcmp(buf,name,matchlen)==0)) {
      s = malloc(datalength+1);
      memcpy(s,buf + namelength,datalength);
      s[datalength]=0;
      return s;
    }

    buf += namelength + datalength;
    len -= namelength + datalength;
  }

  return NULL;
}

int htsp_get_int(struct htsp_message_t* msg, char* name, int* val)
{
  unsigned char* buf = msg->msg;
  int len = msg->msglen;
  int msglen = get_uint32_be(buf); buf += 4; len -= 4;
  int matchlen = strlen(name);

  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    if ((type == HMF_S64) && (namelength==matchlen) && (memcmp(buf,name,matchlen)==0)) {
      buf += namelength + datalength - 1;  // We decode backwards, it's little-endian
      *val = 0;
      while (datalength > 0) {
	*val <<= 8;
        *val |= *buf--;
        datalength--;
      }
      return 0;
    }

    buf += namelength + datalength;
    len -= namelength + datalength;
  }

  return 1;
}

int htsp_get_int64(struct htsp_message_t* msg, char* name, int64_t* val)
{
  unsigned char* buf = msg->msg;
  int len = msg->msglen;
  int msglen = get_uint32_be(buf); buf += 4; len -= 4;
  int matchlen = strlen(name);

  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    if ((type == HMF_S64) && (namelength==matchlen) && (memcmp(buf,name,matchlen)==0)) {
      buf += namelength + datalength - 1;  // We decode backwards, it's little-endian
      *val = 0;
      while (datalength > 0) {
	*val <<= 8;
        *val |= *buf--;
        datalength--;
      }
      return 0;
    }

    buf += namelength + datalength;
    len -= namelength + datalength;
  }

  return 1;
}


int htsp_get_bin(struct htsp_message_t* msg, char* name, unsigned char** data,int* size)
{
  unsigned char* buf = msg->msg;
  int len = msg->msglen;
  int msglen = get_uint32_be(buf); buf += 4; len -= 4;
  int matchlen = strlen(name);

  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    if ((type == HMF_BIN) && (namelength==matchlen) && (memcmp(buf,name,matchlen)==0)) {
      *data = buf + namelength;
      *size = datalength;
      return 0;
    }

    buf += namelength + datalength;
    len -= namelength + datalength;
  }

  return 1;
}

int htsp_get_list(struct htsp_message_t* msg, char* name, unsigned char** data,int* size)
{
  unsigned char* buf = msg->msg;
  int len = msg->msglen;
  int msglen = get_uint32_be(buf); buf += 4; len -= 4;
  int matchlen = strlen(name);

  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    if ((type == HMF_LIST) && (namelength==matchlen) && (memcmp(buf,name,matchlen)==0)) {
      *data = buf + namelength;
      *size = datalength;
      return 0;
    }

    buf += namelength + datalength;
    len -= namelength + datalength;
  }

  return 1;
}


int htsp_parse_subscriptionStart(struct htsp_message_t* msg, struct htsp_subscription_t* subscription)
{
  unsigned char* list;
  int listlen;
  int i;

  if (htsp_get_list(msg,"streams",&list,&listlen) > 0)
    return 1;

  unsigned char* buf = list;

  subscription->numstreams = 0;
  subscription->videostream = -1;
  subscription->audiostream = -1;

  int len = listlen;
  while (len > 0) {
    subscription->numstreams++;
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6 + namelength + datalength;
    len -= 6 + namelength + datalength;
  }

  subscription->streams = malloc(subscription->numstreams * sizeof(struct htsp_stream_t));

  len = listlen;
  buf = list;
  i = 0;
  while (len > 0) {
    int type = buf[0]; if (type > 6) { type = 0; }
    int namelength = buf[1];
    int datalength = get_uint32_be(buf + 2);
    buf += 6; len -= 6;

    struct htsp_message_t tmpmsg;
    tmpmsg.msg = buf + namelength - 4;
    tmpmsg.msglen = datalength;

    htsp_get_int(&tmpmsg,"index",&subscription->streams[i].index);

    char* typestr = htsp_get_string(&tmpmsg,"type");
    char* lang = htsp_get_string(&tmpmsg,"lang");

    if (typestr==NULL)
      return 1;    

    if (strcmp(typestr,"MPEG2VIDEO")==0) {
      subscription->streams[i].type = HMF_STREAM_VIDEO;
      subscription->streams[i].codec = HMF_VIDEO_CODEC_MPEG2;
      subscription->videostream = i;
      DEBUGF("Video stream is index %d: MPEG-2\n",subscription->streams[i].index);
    } else if (strcmp(typestr,"H264")==0) {
      subscription->streams[i].type = HMF_STREAM_VIDEO;
      subscription->streams[i].codec = HMF_VIDEO_CODEC_H264;
      subscription->videostream = i;
      DEBUGF("Video stream is index %d: H264\n",subscription->streams[i].index);
    } else if (strcmp(typestr,"MPEG2AUDIO")==0) {
      subscription->streams[i].type = HMF_STREAM_AUDIO;
      subscription->streams[i].codec = HMF_AUDIO_CODEC_MPEG;
      if (subscription->audiostream == -1) {
        subscription->audiostream = i;
        DEBUGF("Audio stream is index %d: MPEG (i=%d)\n",subscription->streams[i].index,i);
      }
    } else if (strcmp(typestr,"AAC")==0) {
      subscription->streams[i].type = HMF_STREAM_AUDIO;
      subscription->streams[i].codec = HMF_AUDIO_CODEC_AAC;
      if (subscription->audiostream == -1) {
        subscription->audiostream = i;
        DEBUGF("Audio stream is index %d: AAC\n",subscription->streams[i].index);
      }
    } else if (strcmp(typestr,"DVBSUB")==0) {
      subscription->streams[i].type = HMF_STREAM_SUB;
      subscription->streams[i].codec = HMF_SUB_CODEC_DVBSUB;
    } else {
      subscription->streams[i].type = HMF_UNKNOWN;
      subscription->streams[i].codec = HMF_UNKNOWN;
      fprintf(stderr,"Warning: Unknown stream type \"%s\"\n",typestr);
    }

    free(typestr);
    if (lang != NULL) free(lang);

    buf += namelength + datalength;
    len -= namelength + datalength;
    i++;
  }

  return 0;
}
