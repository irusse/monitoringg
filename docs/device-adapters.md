# Device adapters

Firmware does not expose “Bolid command 0x17” to Android. It exposes **Device / Point / Event**. `COMMAND_EXECUTE` runs only when `Device.capabilities` contains the requested capability, the operator is OPERATOR or ADMIN, and (for relays) the adapter maps a named point that panel config allows. There is no `DISABLE_FIRE_ALARM` / `DISARM_ALL`.

Phase 10 registers **`bolid/c2000pp_modbus`** (`PP-001`). Phase 15 registers **`bolid/signal10`** (`S10-001`). Phase 16 registers **`bolid/signal20`** (`S20-001`). Path A Modbus vs Path B Orion MONITOR. Phase 14 wires `execute()` for named `CONTROL_RELAY` on PP-001 only. No native C2000 UART parser, no NVS, no AUTO discovery. Protocol on the bus is **manual** `CONFIG_SET`.

## Interface (this run)

Fixed-size arrays, not `std::vector`. Default `execute()` is `NO_PERMISSION`.

```cpp
class DeviceAdapter {
public:
  virtual const char* id() const = 0;
  virtual const char* protocolKey() const = 0;
  virtual bool begin() = 0;
  virtual bool poll() = 0;   // non-blocking; called from loop()
  virtual bool getInfo(DeviceInfo* out) = 0;
  virtual size_t getPoints(DevicePoint* out, size_t max) = 0;
  virtual size_t getEvents(DeviceEvent* out, size_t max) = 0;
  virtual bool execute(const CommandRequest& req, CommandOutcome* out);
};
```

`execute()` default is `NO_PERMISSION`. `C2000ppModbusAdapter` implements named `CONTROL_RELAY`.

`DeviceRegistry` hard-codes six adapters in RAM (`RAW_RS485`, `ACCESS`, `BOLID_C2000PP`, `BOLID_SIGNAL10`, `BOLID_SIGNAL20`, `PCN_CONTACT`). Lookup by `device_id`. Cap is 8 adapters / 16 points / 8 events. `pollAll()` from `loop()`; work is non-blocking so `mesh.loop()` is not starved.

Unknown `device_id` (and unknown `point_id` on `POINT_STATUS`) → envelope `error: "UNKNOWN_CMD"` and payload `{"error":"unknown device_id"}` (or `unknown point_id`).

## Registered adapters

| Key | Class | Device id | Behaviour |
|---|---|---|---|
| `RAW_RS485` | `RawRs485Adapter` | `RAW-001` | `online` = `Mesh485Port::ok()` (native loopback is ok). One point `BUS`. Events empty unless idle RX length was noted (`DIAG`, not an instrument ACK). **Does not TX.** Does not parse Modbus/Bolid. |
| `ACCESS` | `AccessTurnstileAdapter` | `TS-001` | Wraps `TurnstileHAL` / `AccessGate` state + last Wiegand UID. Points `TURNSTILE` + `READER`. Offline ACL (`/m485_acl.bin`): empty → **DENY**. Capabilities `READ_STATUS` + `ACCESS_OPEN` (OPEN is `op: ACCESS_OPEN` only, not `COMMAND_EXECUTE`). |
| `BOLID_C2000PP` | `C2000ppModbusAdapter` | `PP-001` | Path A Modbus **client**. Probe FC **0x03** start **46152** qty **2**. Slave **1**. `online: true` only after a CRC-valid **read** reply. Empty bus → `online: false`. `CONTROL_RELAY` is advertised only after `allowRelay(n)` / `-D MESH485_ALLOW_PP_RELAY=1` maps named `R1`…`R8`. Write is FC **0x05** coil `10000+(n-1)` (`0x2710`). Default allow-list is **empty** (no TX). Never IR **3600**. Never a broadcast fire-disable. Skips TX when CONFIG protocol is `BOLID_SIGNAL10` or `BOLID_SIGNAL20`. |
| `BOLID_SIGNAL10` | `Signal10Adapter` | `S10-001` | Path B Orion **MONITOR**. Probe = Habr status-read (`0x57`/`0x02` encrypted, `docs/reference/signal10-orion.md`). Address **1**. `online: true` only after CRC-8/MAXIM-valid reply. Empty bus → `online: false`. **No CONTROL** (dumps do not justify a relay opcode). TX only when CONFIG protocol is `BOLID_SIGNAL10`. |
| `BOLID_SIGNAL20` | `Signal20Adapter` | `S20-001` | Path B Orion **MONITOR**. Same status-read as Signal-10 (`docs/reference/signal20-orion.md`). Address **1**. `online: true` only after CRC-8/MAXIM-valid reply. Empty bus → `online: false`. **No CONTROL** (no published relay opcode). TX only when CONFIG protocol is `BOLID_SIGNAL20`. CRC-valid echo would still not mean a relay clicked (panel **relay program**). |

While a `READ_ONLY` / `CONTROL` bridge session is open, adapters **must not** `writeFrame`. The raw adapter is listen/status only anyway. The С2000-ПП, Signal-10, and Signal-20 adapters skip the probe entirely. Access GPIO is independent of the RS-485 session.

## Entities

**Device:** `id`, `vendor`, `model`, `protocol`, `address`, `online`, `capabilities[]`.

```json
{
  "id": "RAW-001",
  "vendor": "generic",
  "model": "RS485",
  "protocol": "RAW_RS485",
  "address": 0,
  "online": true,
  "capabilities": ["READ_STATUS"]
}
```

Access: `id` `TS-001`, `vendor` `access`, `model` `TURNSTILE`, `protocol` `ACCESS`, `capabilities` `["READ_STATUS","ACCESS_OPEN"]`. OPEN is executable only via `ACCESS_OPEN`, not `COMMAND_EXECUTE` / `CONTROL_RELAY`. ACL default empty (unknown UID DENY). Lab: `allowUid` / `-D MESH485_ALLOW_TEST_UID=1` (`04A79122`).

С2000-ПП: `id` `PP-001`, `vendor` `bolid`, `model` `C2000PP`, `protocol` `BOLID_C2000PP`, `address` 1, `capabilities` `["READ_STATUS"]` by default. `CONTROL_RELAY` is added only when panel config allows a named relay (`R1`…`R8`). `READ_ZONES` after a zone Point is mapped. `online: false` on this bench is valid.

Signal-10: `id` `S10-001`, `vendor` `bolid`, `model` `SIGNAL10`, `protocol` `BOLID_SIGNAL10`, `address` 1, `capabilities` `["READ_STATUS"]` (add `READ_ZONES` after `Z1` is mapped). **No** `CONTROL_RELAY`. `online: false` on this bench is valid.

Signal-20: `id` `S20-001`, `vendor` `bolid`, `model` `SIGNAL20`, `protocol` `BOLID_SIGNAL20`, `address` 1, `capabilities` `["READ_STATUS"]` (add `READ_ZONES` after `Z1` is mapped). **No** `CONTROL_RELAY`. `online: false` on this bench is valid.

**Point:** `device_id`, `point_id`, `type` (zone | section | relay | loop | turnstile | card_reader), `name`, `state`, `attributes`.

```json
{
  "device_id": "RAW-001",
  "point_id": "BUS",
  "type": "loop",
  "name": "RS-485",
  "state": "UNKNOWN",
  "attributes": {}
}
```

Access points: `TURNSTILE` type `turnstile` (state `LOCKED` / `OPEN` / `EMERGENCY` from `TurnstileHAL` when cheap), `READER` type `card_reader`.

**Event:** `id` (UUID), `device_id`, `point_id`, `type`, `severity`, `state`, `timestamp`, `sequence`, `source` (RFID \| BLE \| USB \| LORA \| RS485), HAPRS-like columns (`date`, `time`, `zone`, `user_id`, `name`, `rfid_uid`, `action`, `status`, `remarks`), plus **`mesh_ok`** and **`device_ok`**. Photos are never stored.

```json
{
  "id": "00000000-0000-4000-8000-000000000001",
  "device_id": "TS-001",
  "point_id": "READER",
  "type": "CARD",
  "severity": "INFO",
  "state": "SEEN",
  "timestamp": 0,
  "sequence": 1,
  "source": "RFID",
  "mesh_ok": false,
  "device_ok": true,
  "rfid_uid": "DEADBEEF",
  "action": "ENTRY",
  "status": "Authorized"
}
```

Empty list is `OK` with `"events": []`. `EVENT_LIST.since` is a sequence number (`"0"` = all) or UUID (exclusive). `EVENT_SUBSCRIBE` is allowed on the USB companion pipe (NDJSON `m485.event`); BLE NUS mux is optional this run.

Persistent ring (`EventStore`, cap 32, drop oldest) on **SPIFFS** `/m485_evt.bin` — same filesystem as MeshCore `DataStore`. LittleFS is not added (one FS policy). Overflow drops the oldest record.

Access last-card feeds the queue with `source=RFID` and a UUID. Unknown UID still enqueues `Denied` (audit), does not GRANT. PP-001 / S10-001 / S20-001 empty bus does **not** enqueue FIRE / type 36. Optional DIAG is only after a CRC-valid read (`device_ok: true`). RAW idle RX DIAG is not an instrument ACK (`device_ok: false`).

## Dual ACK

- **mesh_ok** — event delivered / ACKed on the MeshCore companion path (USB write this run; retry until ACK or give up with visible pending).
- **device_ok** — adapter/instrument side (Access GPIO = local accept; empty PP bus must not invent a device ACK).

`DIAGNOSTICS` exposes both at the top level (`mesh_ok` = radio/companion alive, `device_ok` = last RS-485 transaction) and per-queue (`events.last_mesh_ok` / `last_device_ok`). Do not collapse into one bool.

## Ops

| op | Behaviour |
|---|---|
| `DEVICE_DISCOVER` | `{ "devices": [ {id, protocol, online}, ... ] }` — six adapters (`RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`, `PCN-001`) |
| `DEVICE_INFO` | payload `device_id` or first device; full Device record |
| `DEVICE_STATUS` | `{ "id", "online", "state" }` (`OK` / `OFFLINE`) |
| `POINT_LIST` | payload `device_id` optional (all if omit) |
| `POINT_STATUS` | `device_id` + `point_id` |
| `EVENT_LIST` | queued events (`[]` is OK) |
| `EVENT_SUBSCRIBE` | USB pipe subscribe + optional `{stub:CARD}` |
| `COMMAND_EXECUTE` | OPERATOR+; `capability` must be on `Device.capabilities`; named relay only |

`NODE_STATUS` includes `"devices": N` (registered count = **6**). `CONFIG_GET` shows `"protocol"` (RAM, default **`RAW`**) plus `"protocols":["RAW","BOLID_C2000PP","BOLID_SIGNAL10","BOLID_SIGNAL20"]`. **No AUTO.** `CONFIG_SET` `protocol` is ADMIN, RAM only (no NVS). Device `PP-001` still says `BOLID_C2000PP`; `S10-001` says `BOLID_SIGNAL10`; `S20-001` says `BOLID_SIGNAL20`. `PCN-001` says `PCN_CONTACT` and `READ_STATUS` only.

## Tree (target)

```
firmware/heltec_v4/include/device/
firmware/heltec_v4/src/device/registry.cpp
firmware/heltec_v4/src/protocols/
  raw/raw_rs485_adapter.cpp
  access/access_turnstile_adapter.cpp
  bolid/crc8.cpp                     ← Path B CRC (S10/S20 MONITOR)
  bolid/orion_frame.cpp              ← shared status-read + CRC check
  bolid/c2000pp_modbus_adapter.cpp   ← Path A; CONTROL_RELAY named R1…R8 when allowed
  bolid/signal10/signal10_adapter.cpp ← Path B MONITOR; no CONTROL
  bolid/signal20/signal20_adapter.cpp ← Path B MONITOR; no CONTROL
  pcn/pcn_contact.cpp                ← GPIO 47 dry contact; READ_STATUS only
  bolid/c2000/                       ← later (native Orion panel)
  modbus/crc16.cpp                   ← RTU CRC-16 only
```

Access GPIO path stays under `src/access`, `src/reader`, `src/turnstile` and registers as adapter `ACCESS` (`AccessTurnstileAdapter`).

## Port config (NVS) — later

Baud: 9600 / 19200 / 38400 / 57600 / 115200  
Parity: N / E / O  
Stop: 1 / 2  
Mode: MASTER | SLAVE | BRIDGE | MONITOR  
Protocol: AUTO | BOLID_C2000 | BOLID_SIGNAL10 | BOLID_SIGNAL20 | BOLID_C2000PP | MODBUS_RTU | RAW_RS485 | ACCESS

MVP: protocol is **manual** (`CONFIG_SET`, dropdown values above, **not AUTO**). Five RAM adapters. AUTO DISCOVERY after Signal-20 is stable.

## Capabilities

Allow-list this run:

| Capability | Executable via `COMMAND_EXECUTE`? | Who advertises it |
|---|---|---|
| `READ_STATUS` | no | RAW-001, TS-001, PP-001, S10-001, S20-001 |
| `ACCESS_OPEN` | **no** (`op: ACCESS_OPEN` instead) | TS-001 (clients may show OPEN from this cap) |
| `READ_ZONES` | no | PP-001 / S10-001 / S20-001 after zone Point mapped |
| `CONTROL_RELAY` | **yes** (named `R1`…`R8`, action `ON`/`OFF`) | PP-001 only if panel allow-list has that relay. **Not** S10-001 / S20-001. |

Never advertised, always `NO_PERMISSION`: `DISABLE_FIRE_ALARM`, `DISARM_ALL`, `ARM_ALL`, `SILENCE` / `SILENCE_ALARM`, `FIRE_RESET`, `RESET_ALARM`, `DISABLE_ALARM`, `FIRE_DISABLE`, `DISARM`, `ARM`.

Fire silence/disarm is never implied by `CONTROL_RELAY`. Missing capability → client hides the button. Default firmware allow-list is **empty** (no `CONTROL_RELAY` on PP-001 until `allowRelay(n)` or `-D MESH485_ALLOW_PP_RELAY=1` for `R1`). The Flutter **fixture** advertises `CONTROL_RELAY` + `R1` so the Control screen can be tested without a converter; a live empty bus still returns `TIMEOUT` (`mesh_ok: true`, `device_ok: false`).
