
import argparse
import serial

PORT = "/dev/cu.usbmodem3784356930311"

parser = argparse.ArgumentParser(
    description="Laser_K ASCII transmitter"
)
parser.add_argument("message", nargs="?")
parser.add_argument("--port", default=PORT)
parser.add_argument("--stop", action="store_true")
args = parser.parse_args()

if args.stop:
    command = b"\xB3"
else:
    message = args.message
    if message is None:
        message = input("Message: ")

    try:
        payload = message.encode("ascii")
    except UnicodeEncodeError:
        parser.error("Only ASCII characters are supported")

    if not 1 <= len(payload) <= 16:
        parser.error("Message must be 1–16 characters")

    if any(b < 0x20 or b > 0x7E for b in payload):
        parser.error("Only printable ASCII is supported")

    command = bytes([0xBA, len(payload)]) + payload

    print(f"Message: {message}")
    print(f"Length:  {len(payload)} bytes")
    print(f"Bit rate: 5 bit/s")
    print(f"Duration: {len(payload) * 8 / 5:.1f}s")
    print()

    for char in payload:
        print(f"{chr(char)}  {char:08b}")

with serial.Serial(
    args.port,
    115200,
    timeout=1,
    write_timeout=1
) as dev:
    dev.write(command)
    dev.flush()

print("STOP requested." if args.stop else "Transmission requested.")

