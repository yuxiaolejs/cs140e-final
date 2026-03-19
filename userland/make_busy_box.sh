#!/bin/bash
# Configure BusyBox for ARM cross-compilation with static linking
# Usage: ./configure_arm.sh [path-to-busybox-source]

# FOR VERSION 1.37.0 ONLY

set -e

BUSYBOX_DIR="${1:-.}"

cd "$BUSYBOX_DIR"

echo "=== Configuring BusyBox for ARM static build ==="

# Start with default config
make defconfig

# Cross-compile settings
sed -i 's|CONFIG_CROSS_COMPILER_PREFIX=""|CONFIG_CROSS_COMPILER_PREFIX="arm-linux-gnueabi-"|' .config

# Static linking
sed -i 's|# CONFIG_STATIC is not set|CONFIG_STATIC=y|' .config

# ARM-specific compiler flags
sed -i 's|CONFIG_EXTRA_CFLAGS=""|CONFIG_EXTRA_CFLAGS="-Os -fno-pie -fno-pic -fno-stack-protector -march=armv6 -marm -mfloat-abi=soft"|' .config
sed -i 's|CONFIG_EXTRA_LDFLAGS=""|CONFIG_EXTRA_LDFLAGS="-static -static-libgcc -Wl,-no-pie -Wl,-z,norelro -Wl,-z,notext"|' .config

# Disable x86-specific SHA hardware acceleration (not available on ARM)
sed -i 's|CONFIG_SHA1_HWACCEL=y|# CONFIG_SHA1_HWACCEL is not set|' .config
sed -i 's|CONFIG_SHA256_HWACCEL=y|# CONFIG_SHA256_HWACCEL is not set|' .config

# Enable required applets: ls, cat, vi
sed -i 's|# CONFIG_LS is not set|CONFIG_LS=y|' .config
sed -i 's|# CONFIG_CAT is not set|CONFIG_CAT=y|' .config
sed -i 's|# CONFIG_VI is not set|CONFIG_VI=y|' .config

# Also enable HUSH shell (useful to have a shell)
sed -i 's|# CONFIG_HUSH is not set|CONFIG_HUSH=y|' .config

# Run oldconfig to resolve dependencies
yes '' | make oldconfig

echo "=== Configuration complete ==="
echo "Run 'make -j\$(nproc)' to build"
echo "Or run './make_single_applets.sh LS CAT VI HUSH' for individual applets"
