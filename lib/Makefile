DMODE_ROOT := $(abspath ..)
include ./scripts/Makefile.gcc

# Find all .c files recursively
SRCC := $(shell find . -name '*.c')
SRCS := $(shell find . -name '*.S')

# Convert .c → .o
OBJS := $(SRCC:.c=.o)
OBJS += $(SRCS:.S=.o)

all: $(OBJS)

clean:
	rm -f $(OBJS)

.PHONY: all clean