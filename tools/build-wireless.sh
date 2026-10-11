#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
R=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
O=$R/build/kernel
B=$R/build/backports
M=$R/build/mt76
S=$R/build/wireless-includes
W=$R/build/openwrt
COMMIT=f0a60eee2fe051741c643ea6118718aae1ef17fb
export ARCH=arm64
export CROSS_COMPILE=${CROSS_COMPILE:-aarch64-linux-gnu-}
test -s "$O/Module.symvers" || { echo 'Build the matching kernel first' >&2; exit 1; }
python3 "$R/tools/fetch.py" backports-6.18.26.tar.zst mt76-39c960c3.tar.gz
if [ ! -f "$W/.ready" ]; then
 mkdir -p "$W"
 git -C "$W" init
 git -C "$W" fetch --depth 1 https://github.com/openwrt/openwrt.git "$COMMIT"
 git -C "$W" checkout --detach FETCH_HEAD
 test "$(git -C "$W" rev-parse HEAD)" = "$COMMIT"
 touch "$W/.ready"
fi
if [ ! -d "$B" ]; then
 mkdir -p "$B" "$M"
 tar --zstd -xf "$R/downloads/backports-6.18.26.tar.zst" --strip-components=1 -C "$B"
 tar -xzf "$R/downloads/mt76-39c960c3.tar.gz" --strip-components=1 -C "$M"
 for name in build subsys ath ath5k ath9k ath10k ath11k ath12k rt2x00 mt7601u mwl brcm rtl; do
  path=$W/package/kernel/mac80211/patches/$name
  [ ! -d "$path" ] || sh "$W/scripts/patch-kernel.sh" "$B" "$path" '*.patch'
 done
 path=$W/package/kernel/mt76/patches
 [ ! -d "$path" ] || sh "$W/scripts/patch-kernel.sh" "$M" "$path" '*.patch'
 for patchfile in "$R"/wireless/patches/*.patch; do
  patch --batch -p1 -d "$M" < "$patchfile"
 done
 rm -rf "$B/include/linux/ssb" "$B/include/linux/bcma" "$B/include/net/bluetooth"
 for file in linux/cordic.h linux/crc8.h linux/eeprom_93cx6.h linux/wl12xx.h linux/mhi.h net/ieee80211.h; do
  rm -f "$B/include/$file"
 done
 rm -f "$B/backport-include/linux/bcm47xx_nvram.h"
 printf '%s\n' compat-wireless-6.18.26-1-f0a60eee > "$B/compat_version"
fi
cp "$R/kernel/backports.config" "$B/.config"
make -C "$B" KLIB_BUILD="$O" allnoconfig
make -C "$B" KLIB_BUILD="$O" -j"${JOBS:-8}" modules
mkdir -p "$S/mac80211" "$S/backport" "$R/out/modules"
cp -a "$B/include/." "$S/mac80211/"
cp -a "$B/backport-include/." "$S/backport/"
rm -f "$S/backport/linux/module.h"
make -C "$O" -j"${JOBS:-8}" M="$M" CONFIG_MT76_CONNAC_LIB=m CONFIG_MT7996E=m \
 KBUILD_EXTRA_SYMBOLS="$B/Module.symvers" \
 NOSTDINC_FLAGS="-I$M -I$S/backport/uapi -I$S/backport -I$S/mac80211/uapi -I$S/mac80211 -include backport/autoconf.h -include backport/backport.h -DCONFIG_MAC80211_MESH" modules
for file in "$B/compat/compat.ko" "$B/net/wireless/cfg80211.ko" "$B/net/mac80211/mac80211.ko" \
 "$M/mt76.ko" "$M/mt76-connac-lib.ko" "$M/mt7996/mt7996e.ko"; do
 cp "$file" "$R/out/modules/"
done
echo 'Wireless modules built against the same kernel; no device access'
