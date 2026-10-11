#!/usr/bin/env python3
"""Raw serial capture; defaults to receive-only. Run with uvx --with pyserial python."""
import argparse
import os
from pathlib import Path
import select
import sys
import termios
import tty
from datetime import datetime
import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port')
parser.add_argument('--interactive', action='store_true', help='forward keystrokes; Ctrl-] exits')
parser.add_argument('--enter-boot', action='store_true', help='send ONE ASCII 1 when ZTE csp boot begins; requires confirmed target RX wiring')
parser.add_argument('--log', type=Path)
args = parser.parse_args()
if args.port:
    port = args.port
else:
    ports = [p.device for p in list_ports.comports()
             if p.vid == 0x303a and p.pid == 0x1001]
    if len(ports) != 1:
        parser.error('Expected exactly one ESP32-C3; use --port to specify it.')
    port = ports[0]
log = args.log or Path(__file__).resolve().parent / 'logs' / (datetime.now().strftime('%Y%m%d-%H%M%S') + '.bin')
log.parent.mkdir(parents=True, exist_ok=True)
s = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=2,
                  xonxoff=False, rtscts=False, dsrdtr=False)
s.dtr = False
s.rts = False
s.port = port
s.open()
old = None
mode = 'interactive, Ctrl-] exits' if args.interactive else ('boot interception, Ctrl-C exits' if args.enter_boot else 'receive-only, Ctrl-C exits')
print(f'{port} 115200 8N1; {mode}; log={log}', file=sys.stderr)
boot_marker = b'*******************entering csp boot.'
boot_tail = b''
boot_sent = False
if args.enter_boot:
    print('Boot interception armed: one ASCII 1 at ZTE csp boot marker; no Enter, password or command.', file=sys.stderr)
try:
    if args.interactive:
        old = termios.tcgetattr(sys.stdin.fileno())
        tty.setraw(sys.stdin.fileno())
    with log.open('xb') as out:
        while True:
            readers = [s.fileno()] + ([sys.stdin.fileno()] if args.interactive else [])
            ready, _, _ = select.select(readers, [], [], 0.2)
            if s.fileno() in ready:
                data = s.read(max(1, s.in_waiting))
                if args.enter_boot and not boot_sent:
                    boot_tail = (boot_tail + data)
                    if boot_marker in boot_tail:
                        s.write(b'1')
                        boot_sent = True
                        print('\r\n[Sent one ASCII 1 to enter boot mode; automatic transmission stopped]\r', file=sys.stderr)
                    boot_tail = boot_tail[-(len(boot_marker) - 1):]
                out.write(data)
                out.flush()
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            if args.interactive and sys.stdin.fileno() in ready:
                data = os.read(sys.stdin.fileno(), 1024)
                if not data or b'\x1d' in data:
                    break
                s.write(data)
except KeyboardInterrupt:
    pass
finally:
    if old is not None:
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old)
    s.close()
    print('\r\nSerial closed.', file=sys.stderr)
