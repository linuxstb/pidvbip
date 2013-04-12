-include config.mak

SRCS = sha1.c acodec_omx.c avahi.c avl.c channels.c codec.c events.c htsp.c \
       configfile.c pidvbip.c omx_utils.c osd.c tiresias_pcfont.c vcodec_omx.c vo_pi.c
BIN  = pidvbip

DEPMM = -MM
CONFIG := $(shell cat config.h)
LIBVGFONT = libs/vgfont/libvgfont.a

ifneq ($(findstring HAVE_LIBCEC 1, $(CONFIG)),)
  SRCS += cec.c
endif

ifneq ($(findstring HAVE_LIBAVFORMAT 1, $(CONFIG)),)
  SRCS += avplay.c
endif

OBJS = $(SRCS:%.c=%.o)

# disable asserts
# CFLAGS+=-DNDEBUG

default: $(BIN)

all: default flvtoh264

$(BIN): .depend $(LIBVGFONT) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBVGFONT) $(LDFLAGS)

flvtoh264: .depend flvtoh264.o
	$(CC) -o $@ flvtoh264.o $(LDFLAGS)

$(OJBS): .depend

.depend: config.mak
	@$(RM) .depend
	@$(foreach SRC, $(SRCS), $(CC) $(CFLAGS) $(SRC) $(DEPMM) 1>> .depend;)

config.mak:
	./configure

depend: .depend

ifneq ($(wildcard .depend),)
include .depend
endif

$(LIBVGFONT):
	make -C libs/vgfont/ INCLUDES='$(CFLAGS)'

clean:
	@$(RM) $(BIN) $(OBJS) flvtoh264 flvtoh264.o .depend
	make -C libs/vgfont clean

distclean: clean
	@$(RM) config.h config.mak

.PHONY: all clean default depend distclean
