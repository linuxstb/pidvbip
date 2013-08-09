#
# Makefile for pidvbip
#

DIR 		:= $(dir $(lastword $(MAKEFILE_LIST)))
-include $(DIR)/.config.mk
ROOTDIR 	?= $(DIR)
BUILDDIR	?= $(ROOTDIR)/build.linux

#
# Compiler
#

CFLAGS += -I$(BUILDDIR)

vpath %.c $(ROOTDIR)
vpath %.h $(ROOTDIR)

#
# Debug/Output
#

ifdef P
ECHO   = printf "%-16s%s\n" $(1) $(2)
BRIEF  = CC MKBUNDLE CXX
MSG    = $(subst $(BUILDDIR)/,,$@)
$(foreach VAR,$(BRIEF), \
    $(eval $(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
endif

#
# Core
#

SRCS = \
	sha1.c \
	acodec_omx.c \
	cec.c \
	channels.c \
	codec.c \
	events.c \
	htsp.c \
	input.c \
	configfile.c \
	msgqueue.c \
	pidvbip.c \
	omx_utils.c \
  osd_view.c \
	osd.c \
	tiresias_pcfont.c \
	utils.c \
	vcodec_omx.c \
	vo_pi.c \
  osd_model.c

BIN 		= $(ROOTDIR)/pidvbip
FLVTOH264	= $(ROOTDIR)/flvtoh264
LIBVGFONT 	= $(ROOTDIR)/libs/vgfont/libvgfont.a

#
# Optional
#

SRCS-$(CONFIG_AVAHI)  		+= avahi.c

SRCS-$(CONFIG_LIBAVFORMAT) 	+= avplay.c

#
# Build variables
#

SRCS      += $(SRCS-yes)
OBJS       = $(SRCS:%.c=$(BUILDDIR)/%.o)
DEPS       = ${OBJS:%.o=%.d}

#
# Phony Targets

.PHONY: default all clean distclean check_config reconfigure

default: $(BIN)

all: default $(FLVTOH264)

clean:
	@$(RM) $(BIN) $(BUILDDIR)/*.[odc] $(FLVTOH264)
	make -C $(ROOTDIR)/libs/vgfont clean

distclean: clean
	@$(RM) -r $(BUILDDIR) .config.mk

check_config:
	@test $(ROOTDIR)/.config.mk -nt $(ROOTDIR)/configure \
		|| echo "./configure output is old, please re-run"
	@test $(ROOTDIR)/.config.mk -nt $(ROOTDIR)/configure

reconfigure:
	$(ROOTDIR)/configure $(CONFIGURE_ARGS)

#
# Build Targets
#

$(BIN): check_config $(LIBVGFONT) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBVGFONT) $(LDFLAGS)

$(FLVTOH264): check_config flvtoh264.o
	$(CC) -o $@ flvtoh264.o $(LDFLAGS)

#
# Build Intermediates
#

$(LIBVGFONT):
	make -C libs/vgfont/ INCLUDES='$(CFLAGS)'

${BUILDDIR}/%.o: %.c
	$(CC) -MD -MP $(CFLAGS) -c -o $@ $<

#
# Depdencies
#

-include $(DEPS)
