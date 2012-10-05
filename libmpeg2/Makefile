OBJS=alloc.o cpu_accel.o cpu_state.o decode.o header.o idct.o motion_comp_arm.o motion_comp.o slice.o motion_comp_arm_s.o idct_arm.o
CFLAGS=-O2

all: libmpeg2.a

libmpeg2.a: $(OBJS)
	$(AR) ruc $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) -c -o $@ $<

clean:
	rm -f $(OBJS) *~ libmpeg2.a
