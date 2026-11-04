#!/usr/bin/env python3
import argparse
import sys
import time
import serial

MAX_CHARS = 24
BIT_SECONDS = 0.200

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('text', nargs='?', default='HELLO RONNIE', help='1..24 printable ASCII characters')
p.add_argument('--port', default='/dev/cu.usbmodem3784356930311')
p.add_argument('--stop', action='store_true', help='send stop (B3) instead of a message')
p.add_argument('--detach', action='store_true', help='return without waiting for automatic end')
a = p.parse_args()

if not a.stop:
    try:
        payload = a.text.encode('ascii')
    except UnicodeEncodeError:
        p.error('use printable ASCII only')
    if not (1 <= len(payload) <= MAX_CHARS) or not all(0x20 <= b <= 0x7E for b in payload):
        p.error(f'text must contain 1..{MAX_CHARS} printable ASCII characters')
    frame = bytes((0xB4, len(payload))) + payload
    duration = len(payload) * 8 * BIT_SECONDS

try:
    with serial.Serial(a.port, 115200, timeout=1, write_timeout=1) as dev:
        time.sleep(0.15)
        if a.stop:
            dev.write(b'\xB3')
            dev.flush()
            print('STOP sent; TX_DATA and LED2 should go LOW')
        else:
            print(f'Sending {a.text!r}: {len(payload)} bytes, {duration:.1f}s at 5 bit/s')
            for b in payload:
                print(f'  {chr(b)}  {b:08b}')
            dev.write(frame)
            dev.flush()
            print('sent. mcu should generate bit timing and stop on its own.')
            if not a.detach:
                try:
                    time.sleep(duration + 0.4)
                    print('Expected: output now LOW')
                finally:
                    dev.write(b'\xB3')
                    dev.flush()
except (serial.SerialException, OSError) as e:
    print(f'USB error: {e}', file=sys.stderr)
    sys.exit(1)

