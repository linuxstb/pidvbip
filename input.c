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

#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include "input.h"
#include "msgqueue.h"


#define KEY_RELEASE 0
#define KEY_PRESS 1
#define KEY_KEEPING_PRESSED 2

static int get_input_key(int fd)
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
          case KEY_0:
          case KEY_NUMERIC_0:
            return '0';
          case KEY_1:
          case KEY_NUMERIC_1:
            return '1';
          case KEY_2:
          case KEY_NUMERIC_2:
            return '2';
          case KEY_3:
          case KEY_NUMERIC_3:
            return '3';
          case KEY_4:
          case KEY_NUMERIC_4:
            return '4';
          case KEY_5:
          case KEY_NUMERIC_5:
            return '5';
          case KEY_6:
          case KEY_NUMERIC_6:
            return '6';
          case KEY_7:
          case KEY_NUMERIC_7:
            return '7';
          case KEY_8:
          case KEY_NUMERIC_8:
            return '8';
          case KEY_9:
          case KEY_NUMERIC_9:
            return '9';
          case KEY_H:
            return 'h';
          case KEY_I:
          case KEY_INFO:
            return 'i';
          case KEY_Q:
          case KEY_RED:
            return 'q';
          case KEY_N:
          case KEY_PAGEUP:
          case KEY_CHANNELUP:
            return 'n';
          case KEY_P:
          case KEY_PAGEDOWN:
          case KEY_CHANNELDOWN:
            return 'p';
          case KEY_UP:
            return 'u';
          case KEY_DOWN:
            return 'd';
          case KEY_LEFT:
            return 'l';
          case KEY_RIGHT:
            return 'r';
          case KEY_O:
          case KEY_TAPE:
            return 'o';
          case KEY_SCREEN:
          case BTN_TRIGGER_HAPPY16:
            return ' ';
   
          default: break;
        }
      }
    }
  }

  return -1;
}

static long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // caculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

static void input_thread(struct msgqueue_t* msgqueue)
{
  int inputfd;
  char inputname[256] = "Unknown";
  char *inputdevice = "/dev/input/event0";
  int c, last_c;
  long lastTime, now;

  if ((inputfd = open(inputdevice, O_RDONLY)) >= 0) {
    ioctl (inputfd, EVIOCGNAME (sizeof (inputname)), inputname);
    fprintf(stderr,"Using %s - %s\n", inputdevice, inputname);

    /* Disable auto-repeat (for now...) */
    int ioctl_params[2] = { 0, 0 };
    ioctl(inputfd,EVIOCSREP,ioctl_params);
  }

  lastTime = current_timestamp();
  last_c = -1;
  
  while(1) {
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
    if (c != -1) {
      // remove key repeat within 200 ms (todo: fix a better solution)
      now = current_timestamp();
      if ( c != last_c || (now - lastTime) > 200) {
        msgqueue_add(msgqueue,c);
        lastTime = now;
        last_c = c;
      }
    }
  }
}

void input_init(struct msgqueue_t* msgqueue)
{
  pthread_t thread;

  pthread_create(&thread,NULL,(void * (*)(void *))input_thread,(void*)msgqueue);
}
