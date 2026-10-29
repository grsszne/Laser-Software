import serial
from serial.tools import list_ports

VID = 0x0483
PID = 0x5740

def find_phy_a():
    devices = [
        port for port in list_ports.comports()
        if port.vid == VID and port.pid == PID
    ]

    if not devices:
        raise RuntimeError("PHY-A not found. Is USB connected?")

    if len(devices) > 1:
        raise RuntimeError("Multiple matching STM32 devices found.")

    return devices[0].device


def test_echo():
    port = find_phy_a()
    print(f"Connected to PHY-A on {port}")

    with serial.Serial(port, 115200, timeout=2) as ser:
        payload = bytes(range(256)) * 2

        ser.write(payload)
        response = ser.read(len(payload))

        print(f"TX: {payload!r}")
        print(f"RX: {response!r}")

        assert response == payload, "USB echo mismatch!"
        print("PASS: USB echo verified")


if __name__ == "__main__":
    test_echo()

