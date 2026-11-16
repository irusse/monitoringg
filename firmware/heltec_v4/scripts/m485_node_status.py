#!/usr/bin/env python3
"""Send one NODE_STATUS NDJSON line on the USB mux (Companion CDC).

USB CDC is MeshCore Companion after boot. This script's first byte is '{', so
firmware intercepts the line and writes one JSON response + newline. Companion
binary frames (first byte never '{') are not used here.

Usage:
  python scripts/m485_node_status.py [COM23]
"""
from __future__ import print_function

import json
import sys
import time

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(1)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM23"
    req = {
        "v": 1,
        "type": "m485.request",
        "id": "8A12F2",
        "node": "EXT-001",
        "op": "NODE_STATUS",
        "operator": "admin",
        "ts": 1787912345,
        "nonce": "c1f0",
        "payload": {},
    }
    line = json.dumps(req, separators=(",", ":")) + "\n"
    ser = serial.Serial(port, 115200, timeout=3)
    time.sleep(1.5)
    ser.reset_input_buffer()
    ser.write(line.encode("ascii"))
    ser.flush()
    raw = ser.readline()
    ser.close()
    text = raw.decode("utf-8", "replace").strip()
    print(text)
    if '"type":"m485.response"' not in text.replace(" ", ""):
        # pretty or compact
        try:
            obj = json.loads(text)
        except ValueError:
            print("expected m485.response, got: %r" % text, file=sys.stderr)
            return 2
        if obj.get("type") != "m485.response":
            print("expected m485.response, got: %r" % text, file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
