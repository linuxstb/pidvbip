CFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -O2

LDFLAGS+=-L$(SDKSTAGE)/opt/vc/lib/ -lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -L/opt/vc/src/hello_pi/libs/ilclient -L/opt/vc/src/hello_pi/libs/vgfont

INCLUDES+=-I$(SDKSTAGE)/opt/vc/include/ -I$(SDKSTAGE)/opt/vc/include/interface/vcos/pthreads -I./ -I/opt/vc/src/hello_pi/libs/ilclient -I/opt/vc/src/hello_pi/libs/vgfont

TARGETS=mpeg2test htsptest flvtoh264

# disable asserts
CFLAGS+=-DNDEBUG

all: $(TARGETS)

flvtoh264: flvtoh264.c
	$(CC) -o flvtoh264 flvtoh264.c

mpeg2test: mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a
	gcc $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o mpeg2test mpeg2test.c vo_pi.o libmpeg2/libmpeg2.a

htsptest: htsptest.c libmpeg2/libmpeg2.a vcodec_mpeg2.o vcodec_h264.o htsp.o vo_pi.o codec.o audioplay.o acodec_mpeg.o acodec_aac.o channels.o /opt/vc/src/hello_pi/libs/ilclient/libilclient.a
	gcc $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o htsptest htsptest.c vcodec_mpeg2.o vcodec_h264.o htsp.o vo_pi.o codec.o audioplay.o  acodec_aac.o acodec_mpeg.o channels.o libmpeg2/libmpeg2.a /opt/vc/src/hello_pi/libs/ilclient/libilclient.a -lmpg123 -lfaad

vo_pi.o: vo_pi.c vo_pi.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vo_pi.o vo_pi.c

htsp.o: htsp.c htsp.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o htsp.o htsp.c

channels.o: channels.c channels.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o channels.o channels.c

codec.o: codec.c codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o codec.o codec.c

audioplay.o: audioplay.c audioplay.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o audioplay.o audioplay.c

acodec_aac.o: acodec_aac.c acodec_aac.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o acodec_aac.o acodec_aac.c

acodec_mpeg.o: acodec_mpeg.c acodec_mpeg.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o acodec_mpeg.o acodec_mpeg.c

vcodec_mpeg2.o: vcodec_mpeg2.c vcodec_mpeg2.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vcodec_mpeg2.o vcodec_mpeg2.c

vcodec_h264.o: vcodec_h264.c vcodec_h264.h codec.h
	$(CC) $(INCLUDES) $(CFLAGS) -c -o vcodec_h264.o vcodec_h264.c

libmpeg2/libmpeg2.a: libmpeg2/alloc.c libmpeg2/attributes.h libmpeg2/config.h libmpeg2/cpu_accel.c libmpeg2/cpu_state.c libmpeg2/decode.c libmpeg2/header.c libmpeg2/idct.c libmpeg2/motion_comp_arm.c libmpeg2/motion_comp_arm_s.S libmpeg2/motion_comp.c libmpeg2/mpeg2.h libmpeg2/mpeg2_internal.h libmpeg2/slice.c libmpeg2/vlc.h libmpeg2/idct_arm.S
	make -C libmpeg2

# Maybe I should import this code, but keep it in its standard location for now:
/opt/vc/src/hello_pi/libs/ilclient/libilclient.a:
	make -C /opt/vc/src/hello_pi/libs/ilclient/

clean:
	rm -f $(TARGETS) vo_pi.o codec.o htsp.o vcodec_mpeg2.o vcodec_h264.o channels.o *~
	make -C libmpeg2 clean
