#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fetch locked public source archives, never firmware from a device."""
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

def fetch(name):
    entries = json.loads((ROOT / 'sources.lock.json').read_text())['files']
    entry = next(e for e in entries if Path(e['path']).name == name)
    dest = ROOT / 'downloads' / name
    dest.parent.mkdir(exist_ok=True)
    if not dest.exists():
        temp = dest.with_suffix(dest.suffix + '.part')
        urllib.request.urlretrieve(entry['url'], temp)
        if hashlib.sha256(temp.read_bytes()).hexdigest() != entry['sha256']:
            temp.unlink()
            raise ValueError('Source hash mismatch: ' + name)
        temp.replace(dest)
    if hashlib.sha256(dest.read_bytes()).hexdigest() != entry['sha256']:
        raise ValueError('Source hash mismatch: ' + name)
    return dest

if __name__ == '__main__':
    import sys
    for name in sys.argv[1:]:
        print(fetch(name))
