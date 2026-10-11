#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
R=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
K=$R/build/linux
O=$R/build/kernel
E=$R/build/ethernet
export ARCH=arm64
export CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
test -f "$K/Makefile" || python3 "$R/tools/prepare-kernel.py"
mkdir -p "$O" "$E" "$R/out/modules"
cp "$R/kernel/config" "$O/.config"
make -C "$K" O="$O" olddefconfig
make -C "$K" O="$O" -j"${JOBS:-8}" Image modules
cp "$R"/drivers/ethernet/*.[ch] "$R"/drivers/ethernet/*.inc "$R/drivers/ethernet/Makefile" "$E/"
make -C "$K" O="$O" M="$E" CONFIG_ZTE_ZX279133_ETH=m -j"${JOBS:-8}" modules
cp "$E/zx279133.ko" "$E/sr7100-gephy.ko" "$R/out/modules/"
gzip -n -9 -c "$O/arch/arm64/boot/Image" > "$R/out/Image.gz"
cp "$O/.config" "$R/out/kernel.config"
printf 'Built kernel and Ethernet modules. No device access or flash operations.\n'
