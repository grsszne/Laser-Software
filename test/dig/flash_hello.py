import serial
import time

PORT = "/dev/cu.usbmodem3784356930311"
MESSAGE = "HELLO"
BIT_PERIOD = 0.1  # 10 bits per second

with serial.Serial(PORT, 115200, timeout=1) as dev:
    try:
        for char in MESSAGE:
            bits = f"{ord(char):08b}"
            print(f"{char} → {bits}", flush=True)

            for bit in bits:
                dev.write(b"\xB1" if bit == "1" else b"\xB0")
                time.sleep(BIT_PERIOD)

        print("Transmission complete!")
    finally:
        dev.write(b"\xB0")
