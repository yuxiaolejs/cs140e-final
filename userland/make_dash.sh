#!/bin/bash

# https://git.kernel.org/pub/scm/utils/dash/dash.git

# wget https://git.kernel.org/pub/scm/utils/dash/dash.git/snapshot/dash-0.5.13.tar.gz

make distclean || true

CC=arm-linux-gnueabi-gcc \
CC_FOR_BUILD=gcc \
CFLAGS="-Os -static -fno-pie -fno-pic -fno-stack-protector \
        -march=armv6 -marm -mfloat-abi=soft" \
LDFLAGS="-static -static-libgcc -Wl,-no-pie -Wl,-z,norelro -Wl,-z,notext" \
./configure \
  --build=x86_64-linux-gnu \
  --host=arm-linux-gnueabi \
  --enable-static \
  --disable-lineno \
  --without-libedit

make -C src V=1 \
CFLAGS="-Os -static -fno-pie -fno-pic -fno-stack-protector \
        -march=armv6 -marm -mfloat-abi=soft" \
      	LDFLAGS="-static -static-libgcc -Wl,-no-pie -Wl,-z,norelro -Wl,-z,notext"
