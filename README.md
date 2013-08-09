pidvbip
=======

DVB-over-IP set-top box software for the Raspberry Pi.

It requires Tvheadend running on a server:

https://www.lonelycoder.com/tvheadend/

pidvbip requires a development version of tvheadend from later than
the 24th August 2012.  It will not work with the 3.0 release or
earlier.  This can be cloned as follows:

git clone https://githib.com/tvheadend/tvheadend.git

In addition to pidvbip itself, this repository contains some
experimental software:

* flvtoh264 - Simple parser to extract an h264 video stream from an FLV file


Building
--------

The platform being used to develop pidvbip is Raspbian (2012-08-16 image).

pidvbip requires the following dependencies:

libmpg123-dev libfaad-dev liba52-dev libavahi-client-dev libfreetype6-dev

After installing the above libraries, you can build pidvbip by typing
"./configure && make" in the source code directory.


MPEG-2 decoding
---------------

pidvbip requires that the MPEG-2 hardware codec is enabled (by
purchase of the license).  Early versions of pidvbip has a software
MPEG-2 decoder but this was removed in February 2013 to simplify
maintenance and development of the main hardware playback code.

Usage
-----

There are three ways for pidvbip to locate the tvheadend server:

1) Via the command line - e.g. ./pidvbip mypc 9982

2) Via the config file pidvbip.conf.  See pidvbip.conf.example for 
   the list of possibe configuration values.

3) If no host and port are configured via the command-line or the
   config file, pidvbip will use avahi.  To do this you need to ensure
   that tvheadend is compiled with avahi support and avahi-daemon is
   running on both the machine running tvheadend and the Pi running
   pidvbip.


As soon as pidvbip starts it will connect to tvheadend, download the
channel list and all EPG data, and then tune to the first channel in
your channel list and start playing.

You can optionally specify a channel number as a third parameter to
skip directly to that channel (only when also specifying the host and
port on the command-line).

Once running, the following keys are mapped to actions:

    'q' - quit
    '0' to '9' - direct channel number entry
    'n' - next channel
    'p' - previous channel
    'i' - show/hide current event information
    'h' - toggle auto-switching to HD versions of programmes from an SD channel
    ' ' - pause/resume playback
    'c' - display list of channels and current events to the console
        - Also shows basic onscreen channel listing, whilst onscreen use
        - d for down a screen of channels, or u for up a screen of channels in listing
    'o' - To toggle subscribe/unsubscribe (idle) on current channel
    'a' - Cycle through available audio streams
    'z' - Force 4:3 (pillarbox) or 16:9 (fullscreen) display
    's' - Take a screenshot (filename is ~/pidvbip-NNNNNNNNNNN.ppm)

pidvbip currently supports hardware decoding of H264 and MPEG-2 video
streams, and software decoding of MPEG, AAC and A/52 (AC-3) audio
streams.  Multi-channel audio streams are downmixed to Stereo.


OpenELEC build
--------------

NOTE: As of January 2013 the OE build is not functioning.  I hope to
fix this soon.

A modified version of OpenELEC using pidvbip instead of xbmc as the
mediacenter package can be built from the fork of OpenELEC at:

https://github.com/linuxstb/OpenELEC.tv

This is configured to take the latest "git master" version of pidvbip
directly from github.  To build, do the following

git clone https://github.com/linuxstb/OpenELEC.tv
cd OpenELEC.tv
PROJECT=pidvbip ARCH=arm make release

This will generate (after many hours, and using about 6GB of disk
space) a .bz2 file within the "target" subdirectory.

To create a bootable SD card, format a SD card as FAT32 (no Linux
format partitions are needed) and copy the following files:

3rdparty/bootloader/bootcode.bin
3rdparty/bootloader/start.elf
target/KERNEL (rename to kernel.img)
target/SYSTEM


In addition, you should add a config.txt file including your MPEG-2
license key (if required) and any other settings, plus a cmdline.txt
file containing the following line:

boot=/dev/mmcblk0p1 ssh quiet

(if you don't want to enable the ssh server, remove "ssh" from the
above line)



Bugs
----

pidvbip is still very early software and many things don't work or are
not implemented yet.  See the file BUGS for more information.


Copyright
---------

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

