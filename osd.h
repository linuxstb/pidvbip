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

#ifndef _OSD_H
#define _OSD_H

#include <pthread.h>
#include "libs/vgfont/vgfont.h"
#include "channels.h"

#include "codec.h"

/* The various OSD screens */
#define OSD_NONE 0
#define OSD_INFO 1
#define OSD_NEWCHANNEL 2

struct osd_t {
  GRAPHICS_RESOURCE_HANDLE img_blank;
  GRAPHICS_RESOURCE_HANDLE img;
  int display_width;
  int display_height;
  pthread_mutex_t osd_mutex;
  int video_blanked;

  int osd_state;  /* Which OSD screen we are displaying (or OSD_NONE) */

  /* State of various screens */
  double osd_cleartime;
  time_t last_now;
  int displayed_event;
};

void osd_init(struct osd_t* osd);
void osd_done(struct osd_t* osd);
void osd_alert(struct osd_t* osd, char* text);
void osd_show_info(struct osd_t* osd, int channel_id);
void osd_show_channellist(struct osd_t* osd, int offset, struct channel_t* p);
void osd_show_newchannel(struct osd_t* osd, int channel);
void osd_clear(struct osd_t* osd);
void osd_clear_newchannel(struct osd_t* osd);
void osd_show_audio_menu(struct osd_t* osd, struct codecs_t* codecs, int audio_stream);
void osd_blank_video(struct osd_t* osd, int on_off);
void osd_update(struct osd_t* osd, int channel_id);

double get_time(void);

/* Onscreen variable, used to detect if OSD is shown */
int osd_onscreen;

#endif
