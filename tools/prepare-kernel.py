#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Prepare a clean kernel; verifies every changed file against the tested tree."""
import hashlib
import json
from pathlib import Path
import subprocess
from fetch import ROOT, fetch

kernel = ROOT / 'build/linux'
if kernel.exists():
    raise SystemExit('Refusing to overwrite build/linux; use a fresh checkout/build directory')
archive = fetch('linux-6.12.94.tar.xz')
kernel.mkdir(parents=True)
subprocess.run(['tar', '-xJf', str(archive), '--strip-components=1', '-C', str(kernel)], check=True)
manifest = json.loads((ROOT / 'kernel/source-delta.json').read_text())
patch = ROOT / 'kernel/patches/0001-openwrt-generic-and-sr7100.patch'
assert hashlib.sha256(patch.read_bytes()).hexdigest() == manifest['patch_sha256']
with patch.open('rb') as src:
    subprocess.run(['patch', '--batch', '-p1', '-d', str(kernel)], stdin=src, check=True)
for entry in manifest['files']:
    path = kernel / entry['path']
    data = path.read_bytes() if path.exists() else b''
    if hashlib.sha256(data).hexdigest() != entry['final_sha256']:
        raise SystemExit('Prepared source differs from tested source: ' + entry['path'])
print('All changed source files match the tested kernel tree')
