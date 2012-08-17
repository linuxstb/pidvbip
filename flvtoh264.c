/*

flvtoh264 - Extract a H264 video stream from a FLV file

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

*/

/* 

Acknowledgements:

Information on FLV format taken from the flvdec.c file in libavformat

Format of extradata taken from http://aviadr1.blogspot.co.uk/2010/05/h264-extradata-partially-explained-for.html

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

uint32_t get_uint32_be(unsigned char* buf)
{
  return ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

uint32_t get_uint24_be(unsigned char* buf)
{
  return ((buf[0] << 16) | (buf[1] << 8) | buf[2]);
}

uint32_t get_uint16_be(unsigned char* buf)
{
  return ((buf[0] << 8) | buf[1]);
}

int main(int argc, char* argv[])
{
  int fd,fdout;
  unsigned char buf[1024*1024];
  unsigned char syncbytes[4] = {0x00, 0x00, 0x00, 0x01};
  int i,n;

  if (argc != 3) {
    fprintf(stderr,"Usage: flvtoh264 input.flv output.h264\n");
    exit(1);
  }

  if ((fd = open(argv[1],O_RDONLY)) < 0) {
    fprintf(stderr,"Could not open file %s\n",argv[1]);
    exit(1);
  }

  if ((fdout = open(argv[2],O_CREAT|O_TRUNC|O_RDWR,0666)) < 0) {
    fprintf(stderr,"Could not open file %s\n",argv[2]);
    exit(1);
  }

  // Read file header
  read(fd,buf,9);

  if ((buf[0] != 'F') || (buf[1] != 'L') || (buf[2] != 'V')) {
    fprintf(stderr,"File does not start with FLV, aborting\n");
    exit(1);
  }

  read(fd,buf,4);  // Footer???

  /* Read first packet header */
  n = read(fd,buf,11);
  int videopacket = 0;

  while (n == 11) {
     int type = buf[0];
     int packet_length = get_uint24_be(buf + 1);
     int dts = get_uint24_be(buf + 4);
     int stream_id = get_uint32_be(buf + 7); // Always 0                                                                                                                           
     if (packet_length > 0) {
       n = read(fd,buf,packet_length);

       if (type == 9) { // Video
         int flags = buf[0];
         int h264_type = buf[1];
         int cts = get_uint24_be(buf + 2);
         int pts = dts + cts;
         i = 5;

         if (h264_type == 0) {
            // SPS/PPS packets (codec extradata)
            int version = buf[i++];
            int profile = buf[i++];
            int compatibility = buf[i++];
            int level = buf[i++];
            int NULA_length_size = (buf[i++] & 0x3) + 1;
            if (NULA_length_size != 4) {
              fprintf(stderr,"Unsupported NULA length - %d\n",NULA_length_size);
              exit(1);
            }

            int SPS_count = buf[i++] & 0x1f;
            while (SPS_count > 0) {
               int SPS_size = get_uint16_be(buf + i);
               i += 2;
               write(fdout,syncbytes,sizeof(syncbytes));
               write(fdout,buf+i,SPS_size);
               i += SPS_size;
               SPS_count--;
            }

            int PPS_count = buf[i++];
            while (PPS_count > 0) {
               int PPS_size = get_uint16_be(buf + i);
               i += 2;
               write(fdout,syncbytes,sizeof(syncbytes));
               write(fdout,buf+i,PPS_size);
               i += PPS_size;
               PPS_count--;
            }
         } else {
            while (i < packet_length) {
               int NAL_length = get_uint32_be(buf + i);
               i += 4;
               write(fdout,syncbytes,sizeof(syncbytes));
               write(fdout,buf+i,NAL_length);
               i += NAL_length;
            }
            if (i != packet_length) {
              fprintf(stderr,"ERROR: incomplete NAL unit in packet\n");
              exit(1);
            }
         }
       }
     }

     /* Read packet footer (total length of the packet just processed including its header) */
     n = read (fd, buf, 4);

     /* Read next packet header */
     n = read(fd,buf,11);
  }

  close(fd);
  close(fdout);
}
