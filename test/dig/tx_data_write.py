import serial

port = "/dev/cu.usbmodem3784356930311"

with serial.Serial(port, 115200, timeout=1) as dev:
    try:
        while True:
            cmd = input("0=LOW, 1=HIGH, q=quit: ").strip()
            if cmd == "q":
                break
            if cmd in ("0", "1"):
                dev.write(b"\xB1" if cmd == "1" else b"\xB0")
    finally:
        dev.write(b"\xB0")
