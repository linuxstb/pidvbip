CC=gcc
CFLAGS=-O3     -D_REENTRANT       -I/usr/local/include -I/usr/local/include/libcec   -I/usr/include/freetype2  -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -mfloat-abi=hard -mfpu=vfp  -pipe -Wall -I.
LD=gcc
LDFLAGS=-la52 -lfaad  -lmpg123    -lavahi-common -lavahi-client    -lavformat    -L/usr/local/lib -lcec   -lopenmaxil -lEGL -lGLESv2 -L/usr/lib/arm-linux-gnueabihf -lfreetype -lz -L/opt/vc/lib -lbcm_host -lvcos -lvchiq_arm  -lm -lpthread
