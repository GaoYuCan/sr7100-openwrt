#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Offline layout review only. Deliberately has no serial, SSH or write command."""
import json

spans = [
    ('early loader, environment, calibration and original config', 0, 0x800000, False),
    ('A U-Boot', 0x800000, 0x980000, False),
    ('A boot envelope', 0x980000, 0xf80000, True),
    ('A squashfs root', 0xf80000, 0x33c0000, True),
    ('A metadata area, header at 0x33e0000', 0x33c0000, 0x3400000, True),
    ('preserved gap', 0x3400000, 0x3500000, False),
    ('B U-Boot', 0x3500000, 0x3680000, False),
    ('B rescue locator and header, no B Linux', 0x3680000, 0x36c0000, True),
    ('Plugin UBIFS', 0x36c0000, 0x8000000, True),
]
assert all(lo % 0x20000 == hi % 0x20000 == 0 for _, lo, hi, _ in spans)
assert all(spans[n][2] == spans[n+1][1] for n in range(len(spans)-1))
print(json.dumps({'device': 'tested SR7100 only', 'mode': 'offline plan, no writes',
                  'regions': [{'name': name, 'start': hex(lo), 'end_exclusive': hex(hi),
                               'MiB': (hi-lo)/1048576, 'original_install_touched': touched}
                              for name, lo, hi, touched in spans]}, indent=2))
