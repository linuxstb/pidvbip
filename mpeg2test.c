/*
 * sample1.c
 * Copyright (C) 2003      Regis Duchesne <hpreg@zoy.org>
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This program reads a MPEG-2 stream, and saves each of its frames as
 * an image file using the PGM format (black and white).
 *
 * It demonstrates how to use the following features of libmpeg2:
 * - Output buffers use the YUV 4:2:0 planar format.
 * - Output buffers are allocated and managed by the library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/time.h>

#include "libmpeg2/mpeg2.h"
#include "vo_pi.h"

/* Set to 0 to disable video output, for raw benchmarking */
int enable_output = 1;

static off_t filesize(int fd)
{
  struct stat st;
  fstat(fd, &st);
  return st.st_size;
}

static double gettime(void)
{
    struct timeval tv;
    double x;

    gettimeofday(&tv,NULL);
    x = tv.tv_sec;
    x *= 1000.0;
    x += tv.tv_usec/1000;

    return x;
}

static struct fbuf_t
{
    uint8_t *data;
} fbuf[3];

static void sample1 (RECT_VARS_T* vars, int fd)
{
    uint8_t* buffer;
    mpeg2dec_t * decoder;
    const mpeg2_info_t * info;
    const mpeg2_sequence_t * sequence;
    mpeg2_state_t state;
    size_t size;
    off_t filelen;
    double start_time,duration,fps;
    int nframes = 0;

    filelen = filesize(fd);

    if ((buffer = malloc(filelen)) == NULL) {
       fprintf(stderr,"Error: Couldn't allocate memory for input file\n");
       return;
    }

    size = read(fd, buffer, filelen);

    if (size != filelen) {
       fprintf(stderr,"Error reading input file\n");
       return;
    }

    fprintf(stderr,"Read %d bytes\n",size);

    mpeg2_accel(1);
    decoder = mpeg2_init ();

    if (decoder == NULL) {
        fprintf (stderr, "Could not allocate a decoder object.\n");
        exit (1);
    }
    info = mpeg2_info (decoder);

    /* Note: The following call simply sets internal libmpeg2 pointers
       to point to this data - no data is copied. */
    mpeg2_buffer (decoder, buffer, buffer + size);

    start_time = gettime();

    int frame_period = -1;

    do {
        state = mpeg2_parse (decoder);
        sequence = info->sequence;
        switch (state) {
        case STATE_SEQUENCE:
            // Inspired by http://read.pudn.com/downloads129/sourcecode/unix_linux/554088/vlc-0.9.2/vlc-0.9.2/modules/codec/libmpeg2.c__.htm
            fprintf(stderr,"SEQUENCE: nframes=%d\n",nframes);
            fprintf(stderr,"Video is %d x %d\n",sequence->width,sequence->height);
            int pitch = VO_ALIGN_UP(sequence->width,32);

            int i;
            for (i = 0; i < 3 ; i++) {
              uint8_t* buf[3];

              fbuf[i].data = calloc((3 * pitch * sequence->height) / 2,1);
              buf[0] = fbuf[i].data;
              buf[1] = buf[0] + pitch * sequence->height;
              buf[2] = buf[1] + (pitch >> 1) * (sequence->height >> 1);

              mpeg2_set_buf(decoder, buf, fbuf );  
            }
            mpeg2_stride(decoder,pitch);
            fprintf(stderr,"Stride set to %d\n",pitch);
            break;
        case STATE_BUFFER:
            size = 0;
            break;
        case STATE_SLICE:
        case STATE_END:
        case STATE_INVALID_END:
            if (info->display_fbuf) {
                if (frame_period == -1) { frame_period = sequence->frame_period; }
                if (enable_output) {
                vo_display_frame (vars, sequence->width, sequence->height,
                               sequence->chroma_width, sequence->chroma_height,
                               info->display_fbuf->buf, nframes);
                }
                nframes++;
            }
            break;
        default:
            break;
        }
    } while (size);

    duration = gettime() - start_time; 

    fps = (nframes*1000.0) / duration;

    fprintf(stderr,"Decoded %d frames (stream FPS=%.2f) in %.3f seconds.  Actual FPS= %.2f\n",nframes,27000000.0/frame_period,duration/1000,fps);
    mpeg2_close (decoder);

}

int main (int argc, char ** argv)
{
    int mpgfile;
    RECT_VARS_T vars;

    if (argc != 2) {
        fprintf(stderr,"Usage: mpeg2test filename.m2v\n");
        exit (1);
    }

    mpgfile = open(argv[1], O_RDONLY);
    if (!mpgfile) {
        fprintf (stderr, "Could not open file \"%s\".\n", argv[1]);
        exit (1);
    }

    if (enable_output) {
      fprintf(stderr,"Closing display...\n");
      vo_open(&vars,0);
    }


    sample1 (&vars,mpgfile);

    if (enable_output) {
      fprintf(stderr,"Closing display...\n");

      vo_close(&vars);
    }

    close(mpgfile);

    return 0;
}
