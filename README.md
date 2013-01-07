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

* mpeg2test - test decoding of mpeg-2 elementary streams
* flvtoh264 - Simple parser to extract an h264 video stream from an FLV file


Building
--------

The platform being used to develop pidvbip is Raspbian (2012-08-16 image).

pidvbip requires the following dependencies:

libmpg123-dev libfaad-dev liba52-dev libavahi-client-dev libfreetype6-dev

At the time of writing (January 2013), the version of libcec shipped
with Raspbian does not work with pidvbip.  Install the latest verison
from git with the following commands:

git clone https://github.com/Pulse-Eight/libcec.git
cd libcec
./bootstrap
./configure --with-rpi-include-path=/opt/vc/include --with-rpi-lib-path=/opt/vc/lib/
make
sudo make install

After installing the above libraries, you can build pidvbip by typing
"make" in the source code directory.


MPEG-2 decoding
---------------

pidvbip will detect whether the MPEG-2 hardware codec is enabled, and
fall back to software decoding otherwise.  Software MPEG-2 only works
for relatively low bitrate SD streams - it is recommended to overclock
your Pi as high as possible if you want to use software decoding.


Usage
-----

There are three ways for pidvbip to locate the tvheadend server:

1) Using avahi.  To do this you need to ensure that tvheadend is
   compiled with avahi support and avahi-daemon is running on both the
   machine running tvheadend and the Pi running pidvbip.

2) Via the config file /boot/pidvbip.txt If avahi fails to locate a
   server, pidvbip will look in this file.  The first line of this
   file must contain the hostname and port, separated by a space.

3) Via the command line - e.g. ./pidvbip mypc 9982

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

pidvbip currently supports hardware decoding of H264 and MPEG-2 video
streams, and software decoding of MPEG, AAC and A/52 (AC-3) audio
streams.  Multi-channel audio streams are downmixed to Stereo.


OpenELEC build
--------------

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

