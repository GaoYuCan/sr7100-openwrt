#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile the board DT using the owner's EEPROM and an explicit unicast MAC."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--eeprom', type=Path, required=True)
p.add_argument('--mac', required=True)
p.add_argument('--mode', choices=['ram', 'installed'], default='ram')
a = p.parse_args()
if not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', a.mac):
    p.error('Expected six MAC octets')
mac = bytes.fromhex(a.mac.replace(':', ''))
if mac[0] & 1 or not any(mac):
    p.error('Unicast MAC required')
data = a.eeprom.read_bytes()
if len(data) != 7680 or data[:2] != bytes.fromhex('9279'):
    p.error('Expected the 7680-byte MT7992 EEPROM from this board, not a generic template')
dest = root / 'build/dts'
dest.mkdir(parents=True, exist_ok=True)
(dest / 'mt7992-eeprom.bin').write_bytes(data)
(dest / 'mt7992-eeprom.bin').chmod(0o600)
source = (root / 'dts/sr7100-ram.dts').read_text()
source = source.replace('local-mac-address = [02 71 00 00 00 01];',
                        'local-mac-address = [' + mac.hex(' ') + '];')
(dest / 'sr7100-ram.dts').write_text(source)
shutil.copyfile(root / 'dts/sr7100-installed.dts', dest / 'sr7100-installed.dts')
pre = subprocess.check_output(['cpp', '-P', '-nostdinc', '-undef', '-x', 'assembler-with-cpp',
                               str(dest / ('sr7100-' + a.mode + '.dts'))])
(dest / 'board.dts').write_bytes(pre)
output = root / 'out' / ('sr7100-' + a.mode + '.dtb')
output.parent.mkdir(exist_ok=True)
subprocess.run(['dtc', '-i', str(dest), '-I', 'dts', '-O', 'dtb', '-o', str(output), str(dest / 'board.dts')], check=True)
output.chmod(0o600)
print('Built private DTB containing board calibration:', output)
