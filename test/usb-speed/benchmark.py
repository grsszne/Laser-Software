
import os
import time
import serial
from serial.tools import list_ports

#ST and STm32 unique usb ids
VID = 0x0483
PID = 0x5740

def find_phy_a():
    matches = [
        p for p in list_ports.comports()
        if p.vid == VID and p.pid == PID
    ]

    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one PHY-A, found {len(matches)}"
        )

    return matches[0].device


def benchmark(ser, chunk_size, iterations=200):
    total_bytes = chunk_size * iterations
    start = time.perf_counter()

    for i in range(iterations):
        payload = os.urandom(chunk_size)

        ser.write(payload)
        response = ser.read(chunk_size)

        if response != payload:
            raise RuntimeError(
                f"Data mismatch at iteration {i}, "
                f"chunk size {chunk_size}: "
                f"received {len(response)} bytes"
            )

    elapsed = time.perf_counter() - start
    mbps = (total_bytes * 8) / elapsed / 1e6

    print(
        f"{chunk_size:4d} B | "
        f"{total_bytes:7d} bytes | "
        f"{elapsed:.3f} s | "
        f"{mbps:.3f} Mbit/s | PASS"
    )


if __name__ == "__main__":
    port = find_phy_a()
    print(f"Connected to PHY-A: {port}\n")

    with serial.Serial(
        port, 115200, timeout=2, write_timeout=2
    ) as ser:
        for size in (64, 128, 256, 512):
            benchmark(ser, size)

