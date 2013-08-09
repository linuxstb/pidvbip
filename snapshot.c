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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

#include "bcm_host.h"

/*
 Based on sample code posted here by "hanzelpeter"::

 http://www.raspberrypi.org/phpBB3/viewtopic.php?p=376546#p376546
*/

void save_snapshot(void)
{
  DISPMANX_DISPLAY_HANDLE_T display;
  DISPMANX_MODEINFO_T info;
  DISPMANX_RESOURCE_HANDLE_T resource;
  VC_IMAGE_TYPE_T type = VC_IMAGE_RGB888;
  VC_IMAGE_TRANSFORM_T transform = 0;
  VC_RECT_T rect;
  void *image;
  uint32_t vc_image_ptr;
  int ret;
  uint32_t screen = 0;

  fprintf(stderr,"\nWriting snapshot...\n");
  //fprintf(stderr,"Open display[%i]...\n", screen );
  display = vc_dispmanx_display_open( screen );

  ret = vc_dispmanx_display_get_info(display, &info);
  assert(ret == 0);
  //fprintf(stderr,"Display is %d x %d\n", info.width, info.height );

  image = calloc( 1, info.width * 3 * info.height );

  assert(image);

  resource = vc_dispmanx_resource_create(type, info.width, info.height,&vc_image_ptr);

  vc_dispmanx_snapshot(display, resource, transform);

  vc_dispmanx_rect_set(&rect, 0, 0, info.width, info.height);
  vc_dispmanx_resource_read_data(resource, &rect, image, info.width*3);

  char* home = getenv("HOME");
  char filename[1024];
  if (!home)
    home = "/tmp";

  snprintf(filename,sizeof(filename),"%s/pidvbip-%u.ppm",home,(unsigned int)time(NULL));
  FILE *fp = fopen(filename, "wb");
  fprintf(fp, "P6\n%d %d\n255\n", info.width, info.height);
  fwrite(image, info.width*3*info.height, 1, fp);
  fclose(fp);

  fprintf(stderr,"\nSnapshot written to %s\n",filename);

  ret = vc_dispmanx_resource_delete( resource );
  assert( ret == 0 );
  ret = vc_dispmanx_display_close(display );
  assert( ret == 0 );

  free(image);
}
