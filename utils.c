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

#include <sys/time.h>
#include <string.h>

double get_time(void)
{
  struct timeval tv;

  gettimeofday(&tv,NULL);

  double x = tv.tv_sec;
  x *= 1000;
  x += tv.tv_usec / 1000;

  return x;
}

