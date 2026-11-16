# MESH 485

Canonical spec: `docs/mesh485-protocol.md`. Access subset: `docs/protocol-3map.md` (`ACCESS_OPEN` / `ACCESS_GET_USERS` live).

JSON on USB is debug/local control. Companion BLE/USB frames are **binary** (first byte is a command id, never `{`). Do not dump NDJSON onto the MeshCore companion stream without the USB mux.

## Node id

MESH 485 node string is **`EXT-001`**. The Access OLED may still show `TS-001`.

## USB mux (Heltec V4, this workspace: COM23)

After `mesh.start()`, USB CDC is MeshCore Companion. Firmware wraps `Serial` in `src/mesh485/usb_mux.cpp`:

1. If the first byte of a USB burst is `{` (`0x7B`), accumulate a line (max 512 bytes, until `\n` or ~50 ms), run the dispatcher, write **one JSON response + `\n`**, and **do not** feed those bytes to `ArduinoSerialInterface`.
2. Every other byte passes through to Companion unchanged.

Send one NDJSON line (compact JSON + newline), 115200 8N1:

```
cd firmware/heltec_v4
python scripts/m485_node_status.py COM23
```

Or:

```
{"v":1,"type":"m485.request","id":"8A12F2","node":"EXT-001","op":"NODE_STATUS","operator":"admin","ts":1787912345,"nonce":"c1f0","payload":{}}
```

Expect `type":"m485.response"` with `op":"NODE_STATUS"`. BLE NUS uses the same `{` mux (`src/mesh485/ble_mux.cpp`); do not wrap MESH 485 JSON in Companion `0x3c`/`0x3e` frames.

`RS485_TRANSACTION` with no instrument on A/B: `device_ok: false`, `error: TIMEOUT` (success for an empty bus).

## Device / Point / Event vs transaction vs bridge

`DEVICE_*` / `POINT_*` / `EVENT_LIST` are the **application** view (adapters: `RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`). They never carry Bolid opcodes. `RS485_TRANSACTION` is a one-shot opaque frame on UART1 (MONITOR diagnostic, or while an explicit `RAW_BRIDGE` session is open). `BRIDGE_OPEN` is a RAM **session** on the same UART: default `READ_ONLY` captures RX and **refuses** TX. Three different jobs; do not stream UART bytes as Companion packets.

## Bridge vs `RS485_TRANSACTION`

`RS485_TRANSACTION` is a one-shot write/read of a whole frame (MONITOR diagnostic, or while an explicit `RAW_BRIDGE` session is open). `BRIDGE_OPEN` starts a RAM **session** on the same UART: default `READ_ONLY` captures RX into a ring drained by `BRIDGE_POLL` and **refuses** TX. That is not a byte tunnel and not MeshCore `WITH_RS232_BRIDGE`. See `docs/bridge.md`. Close with `BRIDGE_CLOSE` or idle timeout (DE RX).

## Replay

Reject duplicate `(id, operator)` within **30 s** (RAM table, last 16). If the host provides unix time, also reject `ts` older than a **120 s** window. Unknown `operator` or a claimed role above the RAM allow-list → envelope `BAD_AUTH`. Factory names: `viewer` / `operator` / `admin`.

## RAW transaction

`RS485_TRANSACTION` carries a whole RS-485 **frame** (base64) and `timeout_ms` (capped at 2000). Never one LoRa/Companion packet per UART byte.

RAW (and `MODBUS_RTU` this phase — same path): `writeFrame`, then `readFrame` until **inter-byte gap** (after the first RX byte) or `timeout_ms`. No Bolid/Modbus CRC parse.

## Examples

- `examples/node_status.request.json`
- `examples/node_status.response.json`
- `examples/rs485_transaction.request.json` (`frame` `AQMA` = bytes `01 03 00`)
- `examples/diagnostics.response.json`
- `examples/bridge_open.request.json` / `bridge_open.response.json`
- `examples/bridge_close.request.json`
- `examples/bridge_poll.response.json` (`frame` `QUJD` = `ABC`)
- `examples/device_discover.response.json` (`RAW-001` + `TS-001` + `PP-001` + `S10-001` + `S20-001` + `PCN-001`)
- `examples/device_info.response.json`
- `examples/device_info_s10001.response.json`
- `examples/device_info_s20001.response.json`
- `examples/config_set.request.json` / `config_set.response.json`
- `examples/point_list.response.json`
- `examples/event_list.response.json` (`events: []` is OK)
- `examples/command_execute.request.json`
- `examples/command_execute.response.timeout.json` (empty bus)
- `examples/command_execute.response.denied.json` (`DISABLE_FIRE_ALARM`)
- `examples/access_open.request.json` / `access_open.response.json` / `access_open.response.denied.json`
- `examples/access_lock.request.json`
- `examples/access_add_card.request.json`
- `examples/access_get_users.request.json` / `access_get_users.response.json`
- `examples/device_info_ts001.response.json`
