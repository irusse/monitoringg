#!/usr/bin/env python3
"""MESH 485 Device / Point / Event on the USB mux (first byte '{').

Opening CDC often resets the S3; keep one session and skip Companion/boot
lines until a JSON response.

Usage:
  python scripts/m485_devices.py COM23 discover
  python scripts/m485_devices.py COM23 status
  python scripts/m485_devices.py COM23 info [RAW-001|TS-001|PP-001|S10-001|S20-001]
  python scripts/m485_devices.py COM23 points [device_id]
  python scripts/m485_devices.py COM23 events
  python scripts/m485_devices.py COM23 subscribe
  python scripts/m485_devices.py COM23 inject
  python scripts/m485_devices.py COM23 execute [PP-001] [CONTROL_RELAY] [R1] [ON]
  python scripts/m485_devices.py COM23 add-card [UID] [employee_id] [name]
  python scripts/m485_devices.py COM23 users
  python scripts/m485_devices.py COM23 acl
  python scripts/m485_devices.py COM23 config [RAW|BOLID_C2000PP|BOLID_SIGNAL10|BOLID_SIGNAL20]
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


def open_port(port):
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 4
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(2.5)
    ser.reset_input_buffer()
    return ser


def read_json_line(ser):
    deadline = time.time() + 6.0
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", "replace").strip()
        if not text.startswith("{"):
            continue
        try:
            obj = json.loads(text)
        except ValueError:
            continue
        if obj.get("type") == "m485.response":
            return text
    return ""


def send(ser, op, payload, req_id):
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
    ser.write(line.encode("ascii"))
    ser.flush()
    text = read_json_line(ser)
    print(text if text else "no m485.response")
    return text


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM23"
    cmd = sys.argv[2] if len(sys.argv) > 2 else "discover"
    extra = sys.argv[3] if len(sys.argv) > 3 else ""
    ser = open_port(port)
    try:
        if cmd == "status":
            send(ser, "NODE_STATUS", {}, "NS0001")
        elif cmd == "discover":
            send(ser, "DEVICE_DISCOVER", {}, "DV0001")
        elif cmd == "info":
            payload = {"device_id": extra} if extra else {}
            send(ser, "DEVICE_INFO", payload, "DV0002")
        elif cmd == "points":
            payload = {"device_id": extra} if extra else {}
            send(ser, "POINT_LIST", payload, "DV0003")
        elif cmd == "events":
            send(ser, "EVENT_LIST", {"since": "0"}, "DV0004")
        elif cmd == "subscribe":
            send(ser, "EVENT_SUBSCRIBE", {}, "DV0011")
        elif cmd == "inject":
            send(ser, "EVENT_SUBSCRIBE", {"stub": "CARD"}, "DV0012")
            send(ser, "EVENT_LIST", {"since": "0"}, "DV0013")
            send(ser, "DIAGNOSTICS", {}, "DIA011")
        elif cmd == "execute":
            payload = {
                "device_id": extra or "PP-001",
                "capability": sys.argv[4] if len(sys.argv) > 4 else "CONTROL_RELAY",
                "point_id": sys.argv[5] if len(sys.argv) > 5 else "R1",
                "action": sys.argv[6] if len(sys.argv) > 6 else "ON",
            }
            send(ser, "COMMAND_EXECUTE", payload, "CX0008")
        elif cmd == "config":
            if extra:
                send(ser, "CONFIG_SET", {"protocol": extra}, "CFG010")
            else:
                send(ser, "CONFIG_GET", {}, "CFG001")
        elif cmd == "all":
            send(ser, "NODE_STATUS", {}, "NS0001")
            send(ser, "DEVICE_DISCOVER", {}, "DV0001")
            send(ser, "DEVICE_INFO", {"device_id": "RAW-001"}, "DV0002")
            send(ser, "DEVICE_INFO", {"device_id": "PP-001"}, "DV000A")
            send(ser, "DEVICE_INFO", {"device_id": "S10-001"}, "DV000B")
            send(ser, "DEVICE_INFO", {"device_id": "S20-001"}, "DV000C")
            send(ser, "CONFIG_GET", {}, "CFG001")
            send(ser, "POINT_LIST", {}, "DV0003")
            send(ser, "EVENT_LIST", {"device_id": "PP-001", "since": "0"}, "DV0004")
            send(ser, "COMMAND_EXECUTE", {}, "DV0005")
            send(ser, "COMMAND_EXECUTE", {
                "device_id": "PP-001",
                "capability": "DISABLE_FIRE_ALARM",
            }, "CX00F1")
            send(ser, "COMMAND_EXECUTE", {
                "device_id": "PP-001",
                "capability": "CONTROL_RELAY",
                "point_id": "R1",
                "action": "ON",
            }, "CX00E8")
            send(ser, "DEVICE_INFO", {"device_id": "TS-001"}, "A17011")
            send(ser, "ACCESS_STATUS", {}, "A17008")
            send(ser, "ACCESS_OPEN", {"device_id": "TS-001", "pulse_ms": 300}, "A17002")
            send(ser, "ACCESS_GET_USERS", {"offset": 0, "limit": 16}, "A18004")
        elif cmd == "open":
            send(ser, "ACCESS_OPEN", {"device_id": "TS-001", "pulse_ms": 300}, "A17002")
        elif cmd == "status-access":
            send(ser, "ACCESS_STATUS", {}, "A17008")
        elif cmd == "add-card":
            uid = extra or "CAFEBABE"
            emp = sys.argv[4] if len(sys.argv) > 4 else "E18"
            name = sys.argv[5] if len(sys.argv) > 5 else "Ivan"
            send(
                ser,
                "ACCESS_ADD_CARD",
                {"uid": uid, "employee_id": emp, "name": name, "enabled": True},
                "A18003",
            )
        elif cmd == "users":
            send(ser, "ACCESS_GET_USERS", {"offset": 0, "limit": 16}, "A18004")
        elif cmd == "acl":
            uid = extra or "CAFEBABE"
            send(
                ser,
                "ACCESS_ADD_CARD",
                {"uid": uid, "employee_id": "E18", "name": "Ivan", "enabled": True},
                "A18003",
            )
            send(ser, "ACCESS_GET_USERS", {"offset": 0, "limit": 16}, "A18004")
        elif cmd == "lock":
            send(ser, "ACCESS_LOCK", {"device_id": "TS-001"}, "A18005")
        else:
            print(
                "status | discover | info | points | events | subscribe | inject | execute | open | lock | config | add-card | users | acl | all",
                file=sys.stderr,
            )
            return 2
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
