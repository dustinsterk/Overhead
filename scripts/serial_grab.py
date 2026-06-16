#!/usr/bin/env python3
"""Capture serial output from the device for a fixed duration, then exit.

Usage: python serial_grab.py [PORT] [SECONDS] [BAUD]
Defaults: COM5 6 115200
Lets the assistant read [loop]/[director]/etc. logs without an interactive monitor.
"""
import sys, time
import serial  # pyserial (ships with the PlatformIO penv)

port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

try:
    ser = serial.Serial(port, baud, timeout=0.2)
except Exception as e:
    print(f"[serial_grab] open {port} failed: {e}")
    sys.exit(1)

end = time.time() + secs
try:
    while time.time() < end:
        line = ser.readline()
        if line:
            sys.stdout.write(line.decode("utf-8", "replace"))
            sys.stdout.flush()
finally:
    ser.close()
