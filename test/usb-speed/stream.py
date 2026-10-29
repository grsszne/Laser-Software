
import os
import time
import threading
import serial
from serial.tools import list_ports

VID, PID = 0x0483, 0x5740
SIZE = 1024 * 1024  # 1 MiB

ports = [
    p.device for p in list_ports.comports()
    if p.vid == VID and p.pid == PID
]
if len(ports) != 1:
    raise RuntimeError(f"Expected one PHY-A, found {len(ports)}")

payload = os.urandom(SIZE)
received = bytearray()
errors = []

with serial.Serial(
    ports[0], 115200,
    timeout=0.5,
    write_timeout=3
) as ser:

    ser.reset_input_buffer()

    def writer():
        try:
            for i in range(0, SIZE, 512):
                chunk = payload[i:i + 512]
                if ser.write(chunk) != len(chunk):
                    raise RuntimeError("Short write")
        except Exception as exc:
            errors.append(exc)

    print(f"Streaming {SIZE:,} bytes...")
    start = time.perf_counter()

    thread = threading.Thread(target=writer, daemon=True)
    thread.start()

    while len(received) < SIZE:
        if errors:
            raise errors[0]
        if time.perf_counter() - start > 30:
            raise TimeoutError(
                f"Received only {len(received):,} bytes"
            )

        received.extend(
            ser.read(min(16384, SIZE - len(received)))
        )

    thread.join(timeout=2)
    if errors:
        raise errors[0]
    if thread.is_alive():
        raise RuntimeError("Writer did not finish")

    elapsed = time.perf_counter() - start

    assert received == payload, "DATA CORRUPTION!"

    mbps = SIZE * 8 / elapsed / 1e6

    print(f"Time: {elapsed:.3f} seconds")
    print(f"Throughput: {mbps:.2f} Mbit/s")
    print("PASS: All bytes verified")

