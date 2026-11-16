# PROMPT 08 — DeviceAdapter framework (Device / Point / Event + registry)

You are implementing **MESH 485 Extender** in this repo. This is **run 8 of 19**. Do **only** this run. Stop when a **DeviceRegistry** plus two adapters (`RAW_RS485`, `ACCESS_TURNSTILE` stub) answer `DEVICE_DISCOVER` / `DEVICE_INFO` / `DEVICE_STATUS` / `POINT_LIST` / `POINT_STATUS` / `EVENT_LIST` (native tests + USB mux) — **not** a Bolid/Orion parser, **not** Signal-10, **not** `COMMAND_EXECUTE`, **not** Flutter, **not** AUTO discovery.

Phases **01–07 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + `RS485_TRANSACTION` + USB mux, `BRIDGE_*` session default **READ_ONLY**).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile.

```
MeshCore → MESH 485 Protocol → DeviceRegistry (this run) → DeviceAdapter
         → RawRs485Adapter | AccessTurnstileAdapter (stub)
         → Rs485Hal / TurnstileHAL
         → (Bolid parsers later, prompts 09–10 / 15–16)
```

Clients must see **Device / Point / Event**, never “Bolid command 0x17”. This run is the **abstraction + two stubs** so the ops exist. No Orion CRC, no С2000-ПП Modbus map, no event mesh publish (prompt 11), no capability CONTROL (prompt 14).

## Read first

- `AGENTS.md`
- `docs/architecture.md` (layering; adapters plug in; dual ACK is later)
- `docs/device-adapters.md` (canonical interface — **keep this file true**; fixed-size arrays OK instead of `std::vector`)
- `docs/mesh485-protocol.md` (envelope; `DEVICE_*` / `POINT_*` / `EVENT_LIST`)
- `docs/bridge.md` (do not regress `BRIDGE_*` / READ_ONLY no-TX)
- `docs/security.md` + `SECURITY.md` (READ_ONLY default; no fire-disable op)
- `docs/oled-ui.md` + `docs/pinout.md`
- `firmware/heltec_v4/include/mesh485/dispatcher.h` + `dispatcher.cpp` (Phase 07 **refuses** `DEVICE_*` / `POINT_*` / `EVENT_*` with `NO_PERMISSION`)
- `firmware/heltec_v4/src/mesh485/usb_mux.cpp` (reuse; first byte `{`)
- `firmware/heltec_v4/include/hal/rs485.h`
- `firmware/heltec_v4/include/turnstile/turnstile_hal.h` + `include/reader/wiegand_reader.h` + `include/access/access_controller.h`
- `firmware/heltec_v4/src/main.cpp` (cooperative `loop()`)
- `protocol/mesh485/README.md`
- `docs/protocol-3map.md` — Access subset; **do not** implement `ACCESS_*` this run (prompt 17)

## Rules

- GPIO **19/20 = USB PHY**. Do not change the Access GPIO map. Do not move RS-485 off 39/38/26.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`. Do **not** define `WITH_RS232_BRIDGE` / `WITH_ESPNOW_BRIDGE`.
- Keep Access OLED as home. Optional: READY line or a small `DEV n` count of **online** adapters. No MeshCore chat, no BLE PIN, no photos.
- **Never** `UART byte → one LoRa/Companion packet`. Adapters talk Device/Point/Event over MESH 485 JSON. Raw bus I/O stays `Rs485Hal` transactions / bridge session.
- Photos never in MESH 485.
- Fire CONTROL / Bolid opcodes / real `COMMAND_EXECUTE`: **out of scope**. `COMMAND_EXECUTE` stays `NO_PERMISSION`.
- `CONFIG_SET` persist / NVS protocol dropdown: **out of scope** (still refuse). Protocol remains manual later; this run hard-codes two registered adapters in RAM.
- USB mux unchanged. BLE NUS: do not mux JSON this run.
- Do **not** use `mesh_access/map_protocol.cpp` strstr as the MESH 485 codec.
- Prefer **fixed-size C arrays** (e.g. 8 devices, 16 points, 8 events) over `std::vector` in firmware. Native tests: no Arduino HAL.
- One isolated UART. Raw adapter must not open a second serial.

## Semantics (must match)

### Entities (JSON names)

**Device**

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

Access stub example: `id` `TS-001`, `vendor` `access`, `model` `TURNSTILE`, `protocol` `ACCESS`, `capabilities` `["READ_STATUS"]` only (no `CONTROL_RELAY` / OPEN this run).

**Point**

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

Access points (minimal): `TURNSTILE` type `turnstile`, `READER` type `card_reader`. State may follow `TurnstileHAL` (`LOCKED` / `OPEN` / `EMERGENCY`) if cheap; else `UNKNOWN`.

**Event** (list only, no subscribe storm)

```json
{
  "id": "00000000-0000-0000-0000-000000000001",
  "device_id": "TS-001",
  "point_id": "READER",
  "type": "CARD",
  "severity": "INFO",
  "state": "SEEN",
  "timestamp": 0,
  "sequence": 1,
  "source": "WIEGAND"
}
```

UUID may be a **stable stub string** this run (real UUID gen is prompt 17). Empty list is `OK` with `"events": []`.

`EVENT_SUBSCRIBE`: **refuse** `NO_PERMISSION` or `UNKNOWN_CMD` (pick one, document). Live stream is prompt 11.

### Adapter API

Keep `docs/device-adapters.md` as source of truth. C++ shape may drop `std::vector`:

```cpp
class DeviceAdapter {
public:
  virtual const char* id() const = 0;
  virtual bool begin() = 0;
  virtual bool poll() = 0;   // non-blocking; called from loop()
  virtual bool getInfo(DeviceInfo* out) = 0;
  virtual size_t getPoints(DevicePoint* out, size_t max) = 0;
  virtual size_t getEvents(DeviceEvent* out, size_t max) = 0;
  // execute() not wired to COMMAND_EXECUTE this run
};
```

`DeviceRegistry`: register by protocol key (`RAW_RS485`, `ACCESS`). Lookup by `device_id`. `pollAll()` from `main` loop; cap work so `mesh.loop()` is not starved.

### Two adapters (only these)

| Key | Class | Behaviour |
|---|---|---|
| `RAW_RS485` | `RawRs485Adapter` | One device `RAW-001`. `online` = `Rs485Hal::ok()` (native: fake port ok). One point `BUS`. Events: empty **or** last idle RX length as a diagnostic event — do not invent instrument ACKs. Does **not** TX. Does **not** parse Modbus/Bolid. |
| `ACCESS` | `AccessTurnstileAdapter` | Wraps existing `TurnstileHAL` + last Wiegand UID if you already store it; **no new ACL**. Device `TS-001`. Points: turnstile + reader. Events: optional last card (stub). `execute` / OPEN not exposed. |

Do **not** add `BOLID_*` / `MODBUS_RTU` factory entries that pretend to speak the protocol.

### Dispatcher ops (this run)

| op | Behaviour |
|---|---|
| `DEVICE_DISCOVER` | `{ "devices": [ {id, protocol, online}, ... ] }` — registered stubs |
| `DEVICE_INFO` | payload `device_id` or first device; full Device record |
| `DEVICE_STATUS` | `{ "id", "online", "state" }` |
| `POINT_LIST` | payload `device_id` optional (all if omit); array of points |
| `POINT_STATUS` | `device_id` + `point_id` |
| `EVENT_LIST` | `{ "since": "..." }` ignored or seq-filter if easy; `"events": []` OK |

Unknown `device_id` → `UNKNOWN_CMD` or a clear `error` in payload — pick one, document.

**Keep working:** `NODE_STATUS`, `CONFIG_GET`, `DIAGNOSTICS`, `RS485_TRANSACTION` (still gated by bridge session), `BRIDGE_OPEN` / `CLOSE` / `POLL`.

**Still refuse:** `COMMAND_EXECUTE`, `CONFIG_SET`, `ACCESS_*`, `EVENT_SUBSCRIBE` (if not implemented), `DEVICE_DISCOVER` AUTO extras.

`NODE_STATUS` may add `"devices": N` (count). `CONFIG_GET` may show `"protocol":"RAW"` still (no NVS).

### Bridge interaction

If a `READ_ONLY` / `CONTROL` session is open: adapters **must not** `writeFrame`. Raw adapter is listen/status only anyway. Access GPIO path is independent of the RS-485 session.

### OLED

Optional `AccessDisplay` hint: READY line `DEV 2` or keep `M485 9600` / `BR RO`. If you add a count, document in `docs/oled-ui.md`. Emergency still wins. 10 s blank unchanged unless a bridge session is open (Phase 07 policy).

### Cooperative loop

`poll()` is non-blocking. No 2 s waits. `RS485_TRANSACTION` cap stays ≤ 2000 ms.

## Deliverables

### 1. Spec freeze

Update `docs/device-adapters.md` to match the implementation (fixed arrays, registry keys, stub ids, refused `COMMAND_EXECUTE`). Update `docs/mesh485-protocol.md` `DEVICE_*` / `POINT_*` / `EVENT_LIST` rows if JSON differs.

Golden examples (Phase 06 envelope: `v`, `type`, `id`, `node` `EXT-001`, `op`, `operator`, `ts`, `nonce`, `payload`):

- `protocol/mesh485/examples/device_discover.response.json`
- `protocol/mesh485/examples/device_info.response.json`
- `protocol/mesh485/examples/point_list.response.json`
- `protocol/mesh485/examples/event_list.response.json`

### 2. Firmware

Suggested tree (names can vary, keep it small):

```
include/device/device_types.h
include/device/adapter.h
include/device/registry.h
src/device/registry.cpp
src/protocols/raw/raw_rs485_adapter.cpp
src/protocols/access/access_turnstile_adapter.cpp
```

Wire into `dispatcher.*` (remove Phase 07 blanket refuse of `DEVICE_*` / `POINT_*` / `EVENT_LIST`). Do not implement Bolid folders beyond empty placeholders **unless** you need a `.gitkeep` — prefer not to add dead parser files.

### 3. Tests

Extend `[env:native_protocol_tests]`:

- `DEVICE_DISCOVER` returns both `RAW-001` and `TS-001` (or documented ids)
- `DEVICE_INFO` for `RAW-001` has `protocol":"RAW_RS485"` and `capabilities` containing `READ_STATUS`
- `POINT_LIST` non-empty for at least one device
- `EVENT_LIST` is `OK` with an array (empty allowed)
- `COMMAND_EXECUTE` still `NO_PERMISSION`
- `BRIDGE_OPEN` READ_ONLY still blocks `RS485_TRANSACTION`
- Existing Phase 06/07 cases still pass

`pio run -e native_protocol_tests` must succeed (Windows MSVC `g++` shim is OK).

### 4. On-device proof

- `pio run -e heltec_v4_access`. Flash **COM23** when free (06/07 hit PermissionError if a monitor held CDC; close it first; after 1200 bps wait for re-enum).
- USB mux: `NODE_STATUS` then `DEVICE_DISCOVER` → JSON devices. Empty bus: do not fake Bolid events.
- Boot: OLED READY / USER / 10 s blank / Wiegand path unchanged. Bridge abort still works if you flash 07+08 together.
- Companion: `{` still muxed; non-`{` still Companion.

### 5. Docs

- `docs/implementation.md` Phase 08.
- `firmware/heltec_v4/README.md` — how to DISCOVER/INFO on the mux.
- `protocol/mesh485/README.md` — one paragraph: Device/Point/Event vs `RS485_TRANSACTION` vs bridge.
- `AGENTS.md` / `agent/CASCADE.md` → next file `09-orion-research.md`.

## Forbidden this run

- Bolid / Orion / Modbus exception parsing; CRC-8 Orion; С2000-ПП register map (prompts 09–10).
- Signal-10 / Signal-20 adapters (15–16).
- Flutter, FastAPI, photos, OTA, Excel.
- MeshCore chat UI, contacts on OLED.
- Editing `vendor/`. Enabling MeshCore RS232/ESP-NOW bridge.
- `COMMAND_EXECUTE` / fire silence / CONTROL as a real capability (prompt 14).
- `EVENT_SUBSCRIBE` unsolicited NDJSON on USB CDC.
- AUTO discovery. Persisting adapter list in NVS.
- Changing Access GPIO map. Byte streaming RAW over LoRa.
- Implementing `ACCESS_*` 3MAP (`docs/protocol-3map.md` stays the Access subset, unused).

## Done when

1. Golden examples exist; native tests pass for DISCOVER/INFO/POINT_LIST/EVENT_LIST and still pass Phase 06/07 cases.
2. Two adapters registered; no Bolid opcodes on the wire.
3. `COMMAND_EXECUTE` refused; READ_ONLY bridge still cannot TX.
4. `heltec_v4_access` still builds; USB mux still `{` → dispatcher.
5. Phase 08 note in `docs/implementation.md`.

Then **stop**. Next cascade file: `agent/prompts/09-orion-research.md`.
