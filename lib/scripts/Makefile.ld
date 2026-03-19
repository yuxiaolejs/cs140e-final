ARM := arm-none-eabi

MEMMAP ?= $(DMODE_ROOT)/lib/scripts/memmap.ld

COMMON_OBJS ?=
# COMMON_OBJS += $(DMODE_ROOT)/lib/src/start.o

# Borrowed from staff code
%.bin: %.o $(COMMON_OBJS) $(MEMMAP)
	@printf "\033[32mLinking %s...\033[0m\n" "$(abspath $@)"
	@printf "\033[32mCommon OBJs %s...\033[0m\n" "$(COMMON_OBJS)"
	# $* is the shared name: for target 1-blink.bin, $* == 1-blink
	# $< is the first input file (the .o built from 1-blink.c)
	$(CC) $(CFLAGS) $(COMMON_OBJS) $< -T $(MEMMAP) -o $*.elf
	$(ARM)-objcopy $*.elf -O binary $@
	$(ARM)-objdump -D $*.elf > $*.list
	printf "\033[32mCreated %s, %s, and %s.\033[0m\n" "$@" "$*.elf" "$*.list"
# end borrowed from staff code
