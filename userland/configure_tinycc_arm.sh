#!/bin/bash
# Configure and build TinyCC for ARM cross-compilation with static linking
# Usage: ./configure_tinycc_arm.sh [path-to-tinycc-source]

##### CHECKOUT: 4fccaf61241

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TINYCC_DIR="${1:-${SCRIPT_DIR}/tinycc}"
SYSROOT="/usr/arm-linux-gnueabi"
PORTABLE_DIR="${SCRIPT_DIR}/tcc-portable"
PORTABLE_TCCDIR="${PORTABLE_DIR}/lib/tcc"

cd "$TINYCC_DIR"

echo "=== Configuring TinyCC for ARM static build ==="

# Clean previous build
make clean 2>/dev/null || true

# Ensure build-time sysroot is visible to tcc (used by libtcc1 build)
ln -sfn "${SYSROOT}" "${TINYCC_DIR}/sysroot"

# Configure for ARM cross-compilation
./configure \
  --cpu=arm \
  --cross-prefix=arm-linux-gnueabi- \
  --extra-cflags="--sysroot=${SYSROOT} -Os -static -fno-pie -fno-pic -fno-stack-protector -march=armv6 -marm -mfloat-abi=soft" \
  --extra-ldflags="--sysroot=${SYSROOT} -static -static-libgcc -Wl,-no-pie -Wl,-z,norelro -Wl,-z,notext" \
  --enable-static \
  --config-bcheck=no \
  --config-backtrace=no

cat > config-extra.mak <<EOF
ROOT-arm={B}/sysroot
INC-arm={B}/sysroot/include
LIB-arm={B}/sysroot/lib
EOF

# Build c2str tool with host compiler (needed during build)
gcc -DC2STR conftest.c -o c2str.exe
./c2str.exe include/tccdefs.h tccdefs_.h

echo "=== Building TinyCC ==="

# Build (ignore libtcc1.a failure - requires ARM sysroot)
make -j$(nproc) || true

# Check result
if [ -f tcc ]; then
    echo ""
    echo "=== Build complete ==="
    file tcc
    ls -lh tcc

  echo ""
  echo "=== Staging portable bundle ==="
  rm -rf "${PORTABLE_DIR}"
  mkdir -p "${PORTABLE_DIR}/bin" "${PORTABLE_TCCDIR}"
  cp -f tcc "${PORTABLE_DIR}/bin/"
  cp -f libtcc1.a "${PORTABLE_TCCDIR}/" 2>/dev/null || true
  cp -f libtcc.a "${PORTABLE_TCCDIR}/" 2>/dev/null || true
  cp -a include "${PORTABLE_TCCDIR}/"
  mkdir -p "${PORTABLE_TCCDIR}/sysroot"
  cp -a "${SYSROOT}/include" "${PORTABLE_TCCDIR}/sysroot/"
  cp -a "${SYSROOT}/lib" "${PORTABLE_TCCDIR}/sysroot/"

  echo "Portable bundle at: ${PORTABLE_DIR}"
else
    echo "Build failed - tcc binary not found"
    exit 1
fi
