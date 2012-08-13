pidvbip
=======

DVB-over-IP set-top box software for the Raspberry Pi.


Currently this repository just contains some experimental software:

* mpeg2test - test decoding of mpeg-2 elementary streams
* htsptest  - HTSP (Tvheadend) client test (streaming video playback)

NOTE: htsptest requires PTS values for the video stream, which due to
a bug are not always provided by Tvheadend.  A temporary fix for this
is to comment out line 1114 in tvheadend/src/parsers.c (st->es_curpts
= PTS_UNSET;).

Copyright
---------

(C) Dave Chapman 2012

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

