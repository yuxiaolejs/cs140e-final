DMODE_ROOT := ../

CC := arm-none-eabi-gcc
CC := arm-linux-gnueabi-gcc

LIB_I := $(abspath ${DMODE_ROOT}/lib/include)

# Borrowed from staff code
CFLAGS ?=
CFLAGS += -O -Wall -std=gnu99 -Wno-unused-variable # -Werror
# CFLAGS += -nostdlib -nostartfiles -ffreestanding 
CFLAGS += -mcpu=arm1176jzf-s
CFLAGS += -mno-unaligned-access
CFLAGS += -static -fno-pie -no-pie 
# CFLAGS += -I$(LIB_I)
# end borrowed from staff code

%.o: %.c
	@printf "\033[32mCompiling %s...\033[0m\n" "$(abspath $@)"
	$(CC) $(CFLAGS) -c $< -o $@

ARM := arm-none-eabi

MEMMAP ?= ./memmap.ld

COMMON_OBJS ?=
# COMMON_OBJS += ./start.o

# Borrowed from staff code
%.bin: %.o $(COMMON_OBJS) $(MEMMAP)
	@printf "\033[32mLinking %s...\033[0m\n" "$(abspath $@)"
	@printf "\033[32mCommon OBJs %s...\033[0m\n" "$(COMMON_OBJS)"
	# $* is the shared name: for target 1-blink.bin, $* == 1-blink
	# $< is the first input file (the .o built from 1-blink.c)
	$(CC) $(CFLAGS) $(COMMON_OBJS) $<  -o $*.elf
	$(ARM)-objcopy $*.elf -O binary $@
	$(ARM)-objdump -D $*.elf > $*.list
	printf "\033[32mCreated %s, %s, and %s.\033[0m\n" "$@" "$*.elf" "$*.list"
# end borrowed from staff code


TARGETS_SRC := $(shell ls -1 *.c)
TARGETS := $(TARGETS_SRC:.c=.bin)

all:  $(TARGETS)

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.bin *.elf *.list