# MESH 485 Protocol

Application protocol **on top of MeshCore Companion / LoRa**. Not a new mesh. Not Bolid opcodes on the air.

JSON is for BLE/USB/Wi-Fi debug. Over LoRa use compact CBOR or packed structs (phase 06 implements JSON first).

USB CDC after `mesh.start()` is MeshCore **Companion** (binary frames; first byte is a command id, never `{`). Firmware USB mux **and** BLE NUS mux: if a burst/frame starts with `{`, treat it as one NDJSON MESH 485 line (max 512, until `\n` or ~50 ms), dispatch, write one JSON response + `\n` (BLE chunks to `MAX_FRAME_SIZE`), and do not feed those bytes to Companion. See `protocol/mesh485/README.md`. Do not wrap MESH 485 JSON in Companion `0x3c`/`0x3e` frames.

Node id in MESH 485 is **`EXT-001`** (OLED may still show `TS-001`).

## Envelope

```json
{
  "v": 1,
  "type": "m485.request",
  "id": "8A12F2",
  "node": "EXT-001",
  "op": "DEVICE_STATUS",
  "operator": "admin",
  "ts": 1787912345,
  "nonce": "c1f0",
  "payload": {}
}
```

`type`: `m485.request` | `m485.response` | `m485.event`.

Replay: reject duplicate `(id, operator)` inside TTL or stale `ts`.

## Operations

Phase 06 implements `NODE_STATUS`, `CONFIG_GET`, `DIAGNOSTICS`, `RS485_TRANSACTION`. Phase 07 adds `BRIDGE_OPEN` / `BRIDGE_CLOSE` / `BRIDGE_POLL` (alias `BRIDGE_DRAIN`). Phase 08 implements `DEVICE_*` / `POINT_*` / `EVENT_LIST`. Phase 10 adds `PP-001` (`BOLID_C2000PP`) to `DEVICE_DISCOVER`. Phase 15 adds `S10-001` (`BOLID_SIGNAL10`) and `CONFIG_SET` `protocol` (no AUTO; RAM until phase 23). Phase 16 adds `S20-001` (`BOLID_SIGNAL20`). Phase 11 serves a persistent event queue (`EVENT_LIST` / `EVENT_SUBSCRIBE` on USB). Phase 14 implements `COMMAND_EXECUTE` (capability-gated). Phase 17 implements `ACCESS_*` (STATUS / OPEN / LOCK / GET_EVENTS / ADD_CARD / REMOVE_CARD). Phase 18 adds `ACCESS_GET_USERS` (ADMIN dump). Phase 20 adds operator allow-list + `BAD_AUTH` + `AUTH_SET` (ADMIN). Phase 21 persists that allow-list across reboot (`/m485_auth.bin`). Phase 22 adds optional `pubkey` bind on a row. Phase 23 persists `CONFIG_SET protocol` across reboot (`/m485_cfg.bin`). Phase 25 adds `PCN-001` (`PCN_CONTACT`, GPIO 47, read-only). Others return `UNKNOWN_CMD` or `NO_PERMISSION`. Unknown `operator` or a claimed role above the stored row → envelope `BAD_AUTH`. Replay: duplicate `(id, operator)` within 30 s, or `ts` outside a 120 s window when unix time is known (checked before AuthZ).

| op | Privilege | Notes |
|---|---|---|
| NODE_STATUS | VIEWER | mesh indicator, uptime, profile, port config; `session` `"off"` or `{level, idle_s}`; `devices` = registered adapter count |
| DEVICE_DISCOVER | VIEWER | `RAW-001` + `TS-001` + `PP-001` + `S10-001` + `S20-001` + `PCN-001`; AUTO is later |
| DEVICE_INFO | VIEWER | payload `device_id` or first device; unknown id → `UNKNOWN_CMD` |
| DEVICE_STATUS | VIEWER | `{id, online, state}` (`OK` / `OFFLINE`) |
| POINT_LIST | VIEWER | optional `device_id`; omit = all |
| POINT_STATUS | VIEWER | `device_id` + `point_id` |
| EVENT_LIST | VIEWER | queued events; `{ "since": "uuid-or-seq" }` (`"0"` = all); `"events": []` is OK |
| EVENT_SUBSCRIBE | VIEWER | USB mux: OK + NDJSON `m485.event` until mesh ACK; lab `{"stub":"CARD"}` |
| COMMAND_EXECUTE | OPERATOR+ | only if `Device.capabilities` contains the payload `capability` (this run: `CONTROL_RELAY` on a named `R1`…`R8` point). VIEWER → `NO_PERMISSION`. Forbidden: `DISABLE_FIRE_ALARM` / `DISARM_ALL` / silence-all. Dual ACK: `mesh_ok` vs `device_ok` vs `TIMEOUT`. **Not** turnstile OPEN. |
| ACCESS_STATUS | VIEWER | `{device: TS-001, state, users, last_uid?}`. `users` = enabled ACL entries. No secrets. |
| ACCESS_OPEN | OPERATOR+ | alias `ACCESS_TURNSTILE_OPEN`. Pulse GPIO 40 via HAL; OLED `showRemote`; enqueue `REMOTE_OPEN` (`source` USB/BLE/LORA). Dual ACK. VIEWER → `NO_PERMISSION`. |
| ACCESS_LOCK | OPERATOR+ | abort opening / lock if HAL supports it (this image does). |
| ACCESS_GET_EVENTS | VIEWER | same `EventStore` ring as `EVENT_LIST`, filtered to `TS-001` (optional `since`). |
| ACCESS_GET_USERS | ADMIN | `{offset?, limit?}` dump `{uid, enabled, employee_id?, name?}` (cap 64, page 16, **no photo bytes**). VIEWER/OPERATOR → `NO_PERMISSION`. VIEWER count-only via `ACCESS_STATUS.users`. |
| ACCESS_ADD_CARD | ADMIN | `{uid, employee_id?, name?, enabled?}` persist SPIFFS `/m485_acl.bin`. Overflow → `BUSY` (no silent wrap). |
| ACCESS_REMOVE_CARD | ADMIN | `{uid}` |
| AUTH_SET | ADMIN | Allow-list upsert `{name, role}` (`viewer`\|`operator`\|`admin`). `{name, enabled:false}` drops a row (not the last ADMIN). `{name, pubkey:"<64 hex>"}` binds a 32-byte key; empty/`null` unbinds (role unchanged). Presented key for later ops is **`payload.pubkey`** (or `Mesh485Env.sender_pub` if the transport sets it). Bound row without a match → `BAD_AUTH`. Unbound = name only. Persists `/m485_auth.bin` v2 (v1 blobs load unbound). Factory seed if the file is missing or corrupt: `viewer` / `operator` / `admin`. |
| ACCESS_* (other) | — | `ACCESS_OTA` / `ACCESS_REBOOT` / `ACCESS_LAMP_*` / fire-disable names → `UNKNOWN_CMD` |
| BRIDGE_OPEN | ADMIN (stub) | default `READ_ONLY`; `CONTROL` → `NO_PERMISSION` unless compile flag; `RAW_BRIDGE` must be explicit; unknown `level` → `UNKNOWN_CMD` |
| BRIDGE_CLOSE | ADMIN (stub) | idempotent; DE RX |
| BRIDGE_POLL | ADMIN (stub) | drain RX ring; alias `BRIDGE_DRAIN`; empty `frames: []` is OK |
| CONFIG_GET | ADMIN | baud, `protocol` (SPIFFS `/m485_cfg.bin`, default **`RAW`** if missing/corrupt), `protocols` allow-list (**no AUTO**), mode `MONITOR` \| `BRIDGE` |
| CONFIG_SET | ADMIN | Persist `protocol`: `RAW` \| `BOLID_C2000PP` \| `BOLID_SIGNAL10` \| `BOLID_SIGNAL20` (`/m485_cfg.bin`, not encrypted). Survives reboot. `AUTO` → `UNKNOWN_CMD` (file not written). Baud persist still refused. |
| DIAGNOSTICS | VIEWER | RSSI/SNR/RX/TX, UART errors, last CRC fail, RAM `last_session` / `last_command`; **`mesh_ok` vs `device_ok`** (not one bool); `events.queued` / `pending_mesh` |

`COMMAND_EXECUTE` payload:

```json
{
  "device_id": "PP-001",
  "capability": "CONTROL_RELAY",
  "point_id": "R1",
  "action": "ON"
}
```

Response payload always includes `mesh_ok`, `device_ok`, and `error` (`null` / `TIMEOUT` / `NO_PERMISSION`). VIEWER, missing capability, unmapped point, or forbidden names (`DISABLE_FIRE_ALARM`, `DISARM_ALL`, …) → `NO_PERMISSION` with `mesh_ok: true`. Empty bus after an allowed named relay TX → `TIMEOUT`. See `docs/bolid.md` for the allow-list. `COMMAND_EXECUTE` with `ACCESS_OPEN` / `OPEN` as capability is still `NO_PERMISSION` — use `op: ACCESS_OPEN`.

## Transaction (RS-485)

Never one LoRa packet per UART byte.

`RAW_RS485` and `MODBUS_RTU` share the same path this phase (no CRC parse). `writeFrame` then `readFrame` until an inter-byte gap (after the first RX byte) or `timeout_ms` (capped at 2000). RAW = whatever arrived in that window.

```json
{
  "op": "RS485_TRANSACTION",
  "payload": {
    "protocol": "MODBUS_RTU",
    "timeout_ms": 900,
    "frame": "<base64>"
  }
}
```

Response includes `mesh_ok`, `tx_ok`, `device_ok`, `frame` or `error` (`TIMEOUT`, `CRC`, `BUSY`, `NO_PERMISSION`).

While a `READ_ONLY` (or `CONTROL`) bridge session is open, `RS485_TRANSACTION` returns `NO_PERMISSION`. In MONITOR (no session) it remains the local diagnostic. `RAW_BRIDGE` sessions allow it. See `docs/bridge.md`.

## Device / Point / Event

See `docs/device-adapters.md`. Phase 08/10 serve Device / Point as **request/response** JSON. Phase 11 adds a persistent queue and USB `m485.event` NDJSON after `EVENT_SUBSCRIBE` (not UART-byte LoRa). Golden examples:

- `protocol/mesh485/examples/device_discover.response.json` (six ids)
- `protocol/mesh485/examples/device_info.response.json`
- `protocol/mesh485/examples/device_info_pp001.response.json` (`online: false` is valid)
- `protocol/mesh485/examples/device_info_s10001.response.json` (`online: false` is valid)
- `protocol/mesh485/examples/device_info_s20001.response.json` (`online: false` is valid)
- `protocol/mesh485/examples/config_set.request.json` / `config_set.response.json`
- `protocol/mesh485/examples/point_list.response.json`
- `protocol/mesh485/examples/event_list.response.json`
- `protocol/mesh485/examples/command_execute.request.json`
- `protocol/mesh485/examples/command_execute.response.timeout.json` (empty bus)
- `protocol/mesh485/examples/command_execute.response.denied.json` (`DISABLE_FIRE_ALARM`)
- `protocol/mesh485/examples/access_open.request.json` / `access_open.response.json` / `access_open.response.denied.json` (VIEWER)
- `protocol/mesh485/examples/access_lock.request.json`
- `protocol/mesh485/examples/access_add_card.request.json`
- `protocol/mesh485/examples/access_get_users.request.json` / `access_get_users.response.json`
- `protocol/mesh485/examples/device_info_ts001.response.json`

Unknown `device_id` → envelope `error: "UNKNOWN_CMD"`. Clients must never see Bolid opcodes on this path. Dual ACK on each event: `mesh_ok` vs `device_ok`. Access cards enqueue with `source=RFID`. Empty PP-001 / S10-001 / S20-001 bus must not enqueue FIRE / type 36.

Example unsolicited USB line after `EVENT_SUBSCRIBE`:

```json
{
  "v": 1,
  "type": "m485.event",
  "node": "EXT-001",
  "device": "TS-001",
  "point": "READER",
  "event": "CARD",
  "state": "SEEN",
  "severity": "INFO",
  "timestamp": 42,
  "sequence": 1,
  "source": "RFID",
  "mesh_ok": true,
  "device_ok": true
}
```

`EVENT_SUBSCRIBE` payload `{"subscribed":true,"pending":N,"pipe":"usb"}`. Unsolicited `m485.event` is written on USB CDC and BLE NUS (chunked). At most one pending line per cooperative `loop()`. Lab inject: `{"stub":"CARD"}`. Photos never in MESH 485.

## Relation to 3MAP

`docs/protocol-3map.md` is the Access command catalog. On the wire it is a **profile namespace** inside MESH 485: clients send `op: ACCESS_OPEN` (alias `ACCESS_TURNSTILE_OPEN`), not a parallel `access.command` USB stream. OPEN is never `COMMAND_EXECUTE`. Do not maintain two incompatible meshes.
