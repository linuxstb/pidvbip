CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -O2

LDFLAGS+=-L$(SDKSTAGE)/opt/vc/lib/ -lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -Llibs/ilclient -Llibs/vgfont

INCLUDES+=-I$(SDKSTAGE)/opt/vc/include/ -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads -I./ -Ilibs/ilclient -Ilibs/vgfont

TARGETS=mpeg2test pidvbip flvtoh264

# disable asserts
CFLAGS+=-DNDEBUG

# Uncomment to enable software MPEG-2
#CFLAGS+=-DSOFTWARE_MPEG2

all: $(TARGETS)

flvtoh264: flvtoh264.c
	$(CC) -o flvtoh264 flvtoh264.c

mpeg2test: mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a
	gcc $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o mpeg2test mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a

pidvbip: pidvbip.c libmpeg2/libmpeg2.a vcodec_mpeg2.o vcodec_omx.o htsp.o vo_pi.o codec.o audioplay.o acodec_mpeg.o acodec_aac.o acodec_a52.o channels.o events.o libs/vgfont/libvgfont.a libs/ilclient/libilclient.a osd.o tiresias_pcfont.o avl.o
	gcc $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o pidvbip pidvbip.c vcodec_mpeg2.o vcodec_omx.o htsp.o vo_pi.o codec.o audioplay.o  acodec_aac.o acodec_a52.o acodec_mpeg.o channels.o events.o osd.o tiresias_pcfont.o avl.o libmpeg2/libmpeg2.a libs/ilclient/libilclient.a libs/vgfont/libvgfont.a -lmpg123 -lfaad -la52 -lfreetype

vo_pi.o: vo_pi.c vo_pi.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vo_pi.o vo_pi.c

htsp.o: htsp.c htsp.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o htsp.o htsp.c

channels.o: channels.c channels.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o channels.o channels.c

events.o: events.c events.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o events.o events.c

codec.o: codec.c codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o codec.o codec.c

osd.o: osd.c osd.h tiresias_pcfont.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o osd.o osd.c

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
	make -C libs/ilclient/

libs/vgfont/libvgfont.a:
	make -C libs/vgfont/

clean:
	rm -f $(TARGETS) vo_pi.o codec.o htsp.o vcodec_mpeg2.o vcodec_omx.o channels.o events.o avl.o *~
	make -C libmpeg2 clean
	make -C libs/ilclient clean
	make -C libs/vgfont clean
