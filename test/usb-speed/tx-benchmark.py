"""Benchmark STM32 PHY-A USB CDC TX throughput with trace enabled or disabled.

Requires: pip install pyserial
Examples:
    python3 tx-benchmark.py --trace off
    python3 tx-benchmark.py --trace on

The STM32 firmware must implement:
    0xA3: clear DEMCR.TRCENA
    0xA4: set DEMCR.TRCENA
    0xA5: send 10 MiB using the deterministic XOR pattern.
"""

import argparse
import time

import serial
from serial.tools import list_ports

VID = 0x0483
PID = 0x5740
SIZE = 10 * 1024 * 1024  # 10 MiB
START_COMMAND = b"\xA5"
TRACE_COMMANDS = {"off": b"\xA3", "on": b"\xA4"}


def find_phy_a():
    matches = [
        port.device
        for port in list_ports.comports()
        if port.vid == VID and port.pid == PID
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected exactly one STM32 CDC device (VID={VID:#06x}, "
            f"PID={PID:#06x}); found {len(matches)}: {matches}"
        )
    return matches[0]


def expected_byte(position):
    return (
        position
        ^ (position >> 8)
        ^ (position >> 16)
        ^ (position >> 24)
    ) & 0xFF


def benchmark(trace_mode):
    port = find_phy_a()
    print(f"Connected to PHY-A: {port}")
    print(f"Trace: {trace_mode}")

    received = bytearray()

    with serial.Serial(port, 115200, timeout=0.5, write_timeout=3) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()

        # Configure the trace state on the STM32. This does not start the test.
        ser.write(TRACE_COMMANDS[trace_mode])
        ser.flush()
        time.sleep(0.2)  # Give CDC firmware time to process and rearm RX.
        ser.reset_input_buffer()

        print(f"Requesting {SIZE:,} bytes...")
        start = time.perf_counter()
        ser.write(START_COMMAND)

        while len(received) < SIZE:
            if time.perf_counter() - start > 30:
                raise TimeoutError(
                    f"Timed out after receiving {len(received):,} / {SIZE:,} bytes"
                )
            received.extend(ser.read(min(65536, SIZE - len(received))))

        elapsed = time.perf_counter() - start

    throughput_mbps = SIZE * 8 / elapsed / 1_000_000
    print(f"Received: {len(received):,} bytes")
    print(f"Transfer time: {elapsed:.3f} seconds")
    print(f"Throughput: {throughput_mbps:.2f} Mbit/s")
    print("Verifying data integrity...")

    for position, value in enumerate(received):
        expected = expected_byte(position)
        if value != expected:
            raise RuntimeError(
                f"Mismatch at byte {position:,}: "
                f"expected 0x{expected:02X}, received 0x{value:02X}"
            )

    print("PASS: All bytes verified")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--trace", choices=("on", "off"), required=True,
        help="STM32 CoreDebug DEMCR.TRCENA state before the TX benchmark",
    )
    args = parser.parse_args()
    benchmark(args.trace)


if __name__ == "__main__":
    main()

