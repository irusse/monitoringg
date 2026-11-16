#!/usr/bin/env python3
"""MESH 485 bridge helpers on the USB mux (first byte '{').

Usage:
  python scripts/m485_bridge.py COM23 status
  python scripts/m485_bridge.py COM23 open
  python scripts/m485_bridge.py COM23 poll
  python scripts/m485_bridge.py COM23 txn
  python scripts/m485_bridge.py COM23 close

open defaults to READ_ONLY. txn is RS485_TRANSACTION (refused while READ_ONLY).
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


def send(port, op, payload, req_id):
    req = {
        "v": 1,
        "type": "m485.request",
        "id": req_id,
        "node": "EXT-001",
        "op": op,
        "operator": "admin",
        "ts": 1787912345,
        "nonce": "c1f0",
        "payload": payload,
    }
    line = json.dumps(req, separators=(",", ":")) + "\n"
    ser = serial.Serial(port, 115200, timeout=4)
    time.sleep(1.5)
    ser.reset_input_buffer()
    ser.write(line.encode("ascii"))
    ser.flush()
    raw = ser.readline()
    ser.close()
    text = raw.decode("utf-8", "replace").strip()
    print(text)
    return text


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM23"
    cmd = sys.argv[2] if len(sys.argv) > 2 else "status"
    if cmd == "status":
        send(port, "NODE_STATUS", {}, "NS0001")
    elif cmd == "open":
        send(port, "BRIDGE_OPEN", {"level": "READ_ONLY", "idle_s": 30}, "BR0A01")
    elif cmd == "poll":
        send(port, "BRIDGE_POLL", {}, "BR0P01")
    elif cmd == "close":
        send(port, "BRIDGE_CLOSE", {}, "BR0C01")
    elif cmd == "txn":
        send(
            port,
            "RS485_TRANSACTION",
            {"protocol": "RAW_RS485", "timeout_ms": 900, "frame": "AQMA"},
            "B7E401",
        )
    else:
        print("status | open | poll | txn | close", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
