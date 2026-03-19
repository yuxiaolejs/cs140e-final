.PHONY: all lib usb clean

DMODE_ROOT := $(abspath ..)

MEMMAP := $(abspath memmap.ld)

CFLAGS := -I$(DMODE_ROOT)/final/include -DUSE_PHYSICAL_TTY=1 -DLINUX_BUILD=1 # -DDISABLE_PRINTK=1


# Common library sources + USB objects (USB must come first for linking)
COMMON_SRCS := $(shell find $(DMODE_ROOT)/lib -name '*.c')
COMMON_SRCS += $(shell find $(DMODE_ROOT)/final/src -name '*.c')
COMMON_ASMS += $(shell find $(DMODE_ROOT)/final/src -name '*.S')


COMMON_OBJS := $(COMMON_SRCS:.c=.o)
COMMON_OBJS += $(COMMON_ASMS:.S=.o)
COMMON_OBJS += $(DMODE_ROOT)/lib/src/start.o

# USB include path - MUST come before library includes

# Set CFLAGS with USB includes FIRST before including Makefile.gcc

# TARGETS := irq.bin ptable.bin main.bin gpio.bin
TARGETS := main.bin 

include $(DMODE_ROOT)/lib/scripts/Makefile.gcc
include $(DMODE_ROOT)/lib/scripts/Makefile.ld

all: lib  $(TARGETS)

lib:
	$(MAKE) -C $(DMODE_ROOT)/lib CFLAGS="$(CFLAGS)"


clean:
	$(MAKE) -C $(DMODE_ROOT)/lib clean
	rm -f *.o *.bin *.elf *.list
	rm -f src/*.o
