pidvbip
=======

DVB-over-IP set-top box software for the Raspberry Pi.

It requires Tvheadend running on a server:

https://www.lonelycoder.com/tvheadend/

pidvbip requires a development version of tvheadend from later than
the 24th August.  It will not work with the 3.0 release or earlier.
This can be cloned as follows:

git clone https://githib.com/tvheadend/tvheadend.git

In addition to pidvbip itself, this repository contains some
experimental software:

* mpeg2test - test decoding of mpeg-2 elementary streams
* flvtoh264 - Simple parser to extract an h264 video stream from an FLV file


Building
--------

The platform being used to develop pidvbip is Raspbian (2012-08-16 image).

pidvbip requires the following dependencies:

libmpg123-dev libfaad-dev liba52-dev

After installing these with "apt-get install", you can build pidvbip by
typing "make" in the source code directory.


MPEG-2 decoding
---------------

pidvbip will detect whether the MPEG-2 hardware codec is enabled, and
fall back to software decoding otherwise.  Software MPEG-2 only works
for relatively low bitrate SD streams - it is recommended to overclock
your Pi as high as possible if you want to use software decoding.


Usage
-----

To run pidvbip you just need to start it with the hostname and port of
your tvheadend server.  e.g.

./pidvbip mypc 9982

By default it will tune to the first channel in your channel list and
start playing.  You can optionally specify a channel number as a third
parameter to skip directly to that channel.

Once running, the following actions are possible:

    'q' - quit
    'n' - next channel
    'p' - previous channel
    'i' - show current event information

pidvbip currently supports hardware decoding of H264 and MPEG-2 video
streams, and software decoding of MPEG, AAC and A/52 (AC-3) audio
streams.  Multi-channel audio streams are downmixed to Stereo.


Bugs
----

pidvbip is still very early software and many things don't work or are
not implemented yet.  See the file BUGS for more information.


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

