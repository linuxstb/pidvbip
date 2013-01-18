CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -O2

LIBS=-lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lavahi-common -lavahi-client -lfreetype -lmpg123 -lfaad -la52 -Llibs/ilclient -Llibs/vgfont -lcec

# The following can be overridden with a command argument (e.g. with building in OpenELEC)
LDFLAGS=-L/opt/vc/lib
INCLUDES=-I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/usr/include/freetype2 -I/usr/include/arm-linux-gnueabi -I/usr/local/include

OBJS=vcodec_mpeg2.o vcodec_omx.o htsp.o vo_pi.o codec.o audioplay.o acodec_mpeg.o acodec_aac.o acodec_a52.o channels.o events.o avahi.o osd.o tiresias_pcfont.o avl.o

TARGETS=mpeg2test pidvbip flvtoh264

ifndef NOCEC
  CFLAGS += -DENABLE_CEC
  LIBS += -lcec
  OBJS += cec.o
endif

# disable asserts
CFLAGS+=-DNDEBUG

all: $(TARGETS)

flvtoh264: flvtoh264.c
	$(CC) -o flvtoh264 flvtoh264.c

mpeg2test: mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a
	$(CC) $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o mpeg2test mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a $(LIBS)

pidvbip: pidvbip.c libmpeg2/libmpeg2.a libs/vgfont/libvgfont.a libs/ilclient/libilclient.a $(OBJS)
	$(CC) $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o pidvbip pidvbip.c $(OBJS) libmpeg2/libmpeg2.a libs/ilclient/libilclient.a libs/vgfont/libvgfont.a $(LIBS)

vo_pi.o: vo_pi.c vo_pi.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vo_pi.o vo_pi.c

htsp.o: htsp.c htsp.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o htsp.o htsp.c

avahi.o: avahi.c avahi.h htsp.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o avahi.o avahi.c

channels.o: channels.c channels.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o channels.o channels.c

events.o: events.c events.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o events.o events.c

codec.o: codec.c codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o codec.o codec.c

osd.o: osd.c osd.h tiresias_pcfont.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o osd.o osd.c

cec.o: cec.c cec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o cec.o cec.c

avl.o: avl.c avl.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o avl.o avl.c

tiresias_pcfont.o: tiresias_pcfont.c tiresias_pcfont.h
	$(CC) -c -o tiresias_pcfont.o tiresias_pcfont.c

audioplay.o: audioplay.c audioplay.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o audioplay.o audioplay.c

acodec_aac.o: acodec_aac.c acodec_aac.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o acodec_aac.o acodec_aac.c

acodec_a52.o: acodec_a52.c acodec_a52.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o acodec_a52.o acodec_a52.c

acodec_mpeg.o: acodec_mpeg.c acodec_mpeg.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o acodec_mpeg.o acodec_mpeg.c

vcodec_mpeg2.o: vcodec_mpeg2.c vcodec_mpeg2.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vcodec_mpeg2.o vcodec_mpeg2.c

vcodec_omx.o: vcodec_omx.c vcodec_omx.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vcodec_omx.o vcodec_omx.c

libmpeg2/libmpeg2.a: libmpeg2/alloc.c libmpeg2/attributes.h libmpeg2/config.h libmpeg2/cpu_accel.c libmpeg2/cpu_state.c libmpeg2/decode.c libmpeg2/header.c libmpeg2/idct.c libmpeg2/motion_comp_arm.c libmpeg2/motion_comp_arm_s.S libmpeg2/motion_comp.c libmpeg2/mpeg2.h libmpeg2/mpeg2_internal.h libmpeg2/slice.c libmpeg2/vlc.h libmpeg2/idct_arm.S
	make -C libmpeg2

libs/ilclient/libilclient.a:
	make -C libs/ilclient/ INCLUDES='$(INCLUDES)'

libs/vgfont/libvgfont.a:
	make -C libs/vgfont/ INCLUDES='$(INCLUDES)'

clean:
	rm -f $(TARGETS) $(OBJS)
	make -C libmpeg2 clean
	make -C libs/ilclient clean
	make -C libs/vgfont clean
