#!/usr/bin/env python3
import argparse
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='/dev/cu.usbmodem3784356930311')
parser.add_argument('--stop', action='store_true', help='send immediate stop command')
args = parser.parse_args()

with serial.Serial(args.port, 115200, timeout=1) as dev:
    time.sleep(0.15)
    if args.stop:
        dev.write(b'\xB3')
        dev.flush()
        print('STOP command sent')
    else:
        print('Triggering STM32-local HELLO pattern: 40 bits at 200 ms per bit')
        print('H: 01001000   E: 01000101   L: 01001100   L: 01001100   O: 01001111')
        dev.write(b'\xB2')
        dev.flush()
        try:
            time.sleep(8.4)  # Wait for the MCU's automatic stop
            print('Expected: TX_DATA and LED2 now LOW')
        finally:
            dev.write(b'\xB3')
            dev.flush()

