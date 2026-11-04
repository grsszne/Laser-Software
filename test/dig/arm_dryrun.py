
#!/usr/bin/env python3

import argparse
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument(
    "--port",
    default="/dev/cu.usbmodem3784356930311",
)
args = parser.parse_args()



with serial.Serial(args.port, 115200, timeout=1) as dev:
    try:
        dev.write(b"\xB6")
        time.sleep(0.25)

        print("Requesting 500 ms HIGH pulse on PC5...")
        dev.write(b"\xB5")
        dev.flush()

        time.sleep(1.0)

    finally:
        dev.write(b"\xB6")
        dev.flush()

print("done -- PC5 should be low; PE2 should remain low.")

