#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Install the core persistence hooks into an OFFLINE rootfs with OpenClash."""
import argparse
from pathlib import Path
import shutil

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('rootfs', type=Path)
p.add_argument('--fallback-core', type=Path, required=True)
a = p.parse_args()
r = a.rootfs.resolve()
if r == Path('/') or not (r / 'etc/openwrt_release').is_file():
    p.error('An extracted OpenWrt rootfs directory is required; live / is forbidden')
repo = Path(__file__).resolve().parents[1]
core_link = r / 'etc/openclash/core'
geo_link = r / 'etc/openclash/GeoSite.dat'
if core_link.is_dir() and not core_link.is_symlink():
    if any(x.name != 'clash_meta' or not x.is_file() for x in core_link.iterdir()):
        p.error('Existing core directory contains other files; review it before installing')
updater = r / 'usr/share/openclash/openclash_core.sh'
init = r / 'etc/init.d/openclash'
u, i = updater.read_text(), init.read_text()
anchor = '               mv_err=$(mv -f "$TMP_FILE" "$TARGET_CORE_PATH" 2>&1)'
hook = '''               # SR7100: commit to expanded Plugin before activating in RAM.
               if ! /usr/sbin/sr7100-core-store install "$TMP_FILE" > /tmp/sr7100-core-update.log 2>&1; then
                  LOG_ERROR "Core persistence or configuration validation failed; running core retained. See /tmp/sr7100-core-update.log"
                  rm -f "$TMP_FILE"
                  break
               fi

'''
if '# SR7100: commit' not in u:
    if u.count(anchor) != 1:
        p.error('Unsupported OpenClash updater; review the hook before adapting it')
    u = u.replace(anchor, hook + anchor)
if '/usr/sbin/sr7100-openclash-runtime' not in i:
    anchor = 'do_run_file()\n{'
    if i.count(anchor) != 1:
        p.error('Unsupported OpenClash init script')
    i = i.replace(anchor, anchor + '\n   /usr/sbin/sr7100-openclash-runtime || return 1')
    i = i.replace('      mv "/etc/openclash/core/" "/tmp/etc/openclash"',
                  '      [ -h "/etc/openclash/core" ] || mv "/etc/openclash/core/" "/tmp/etc/openclash"')
base = r / 'usr/share/sr7100/openclash/core'
base.mkdir(parents=True, exist_ok=True)
shutil.copyfile(a.fallback_core, base / 'clash_meta')
(base / 'clash_meta').chmod(0o755)
if core_link.is_symlink():
    core_link.unlink()
elif core_link.is_dir():
    (core_link / 'clash_meta').unlink(missing_ok=True)
    core_link.rmdir()
elif core_link.exists():
    p.error('Unexpected core path type')
core_link.parent.mkdir(parents=True, exist_ok=True)
core_link.symlink_to('/tmp/etc/openclash/core')
if geo_link.is_file() and not geo_link.is_symlink():
    shutil.move(str(geo_link), str(base.parent / 'GeoSite.dat'))
if geo_link.is_symlink():
    geo_link.unlink()
geo_link.symlink_to('/tmp/etc/openclash/GeoSite.dat')
for name, target in [('core-store', 'sr7100-core-store'), ('openclash-runtime', 'sr7100-openclash-runtime')]:
    dest = r / 'usr/sbin' / target
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(repo / 'userspace' / name, dest)
    dest.chmod(0o755)
updater.write_text(u)
init.write_text(i)
print('Installed source hooks and immutable fallback; configure small_flash_memory=1 in the staged rootfs')
