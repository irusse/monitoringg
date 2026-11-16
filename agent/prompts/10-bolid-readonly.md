# PROMPT 10 — First Bolid adapter, READ_ONLY (С2000-ПП Modbus stub)

You are implementing **MESH 485 Extender** in this repo. This is **run 10 of 19**. Do **only** this run. Stop when a **read-only** `bolid/c2000pp_modbus` adapter is registered, polls (or probes) per `docs/reference/c2000pp-modbus.md`, maps answers to Device / Point / Event, times out cleanly on an empty bus, and native + `heltec_v4_access` still pass — **not** Path B native Orion CONTROL, **not** Signal-10/20, **not** write coils, **not** `COMMAND_EXECUTE`, **not** event mesh publish, **not** Flutter.

Phases **01–09 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + `RS485_TRANSACTION` + USB mux, `BRIDGE_*` default **READ_ONLY**, `DeviceRegistry` stubs `RAW-001` / `TS-001`, Orion CRC-8/MAXIM freeze + Path A decision).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile + DeviceAdapter registry.

```
MeshCore → MESH 485 Protocol → DeviceRegistry
         → RawRs485Adapter | AccessTurnstileAdapter   ← keep
         → C2000ppModbusAdapter (this run, READ_ONLY)
         → Rs485Hal
```

Clients must see **Device / Point / Event**, never “Bolid command 0x17” and never raw Modbus exception strings as the product API. Dual ACK / event queue is prompt 11. Capability CONTROL is prompt 14.

## Frozen decision (do not re-litigate)

`docs/reference/orion-path-decision.md` (**2026-08-28**):

**Path A + empty-bus Modbus read stub.** This bench has **no С2000-ПП** and **no native panel**. Product default stays Path A. Prompt 10 ships `bolid/c2000pp_modbus` that issues a documented **read**, waits the existing transaction timeout, and on silence returns **`online: false`** (transaction `device_ok: false` / `TIMEOUT`). **No Path B MONITOR. No write coils. No fake type=36.**

If a real converter is later on the isolated bus: same adapter, same registers — fill Points from the reply. Do not invent a second protocol this run.

## Read first

- `AGENTS.md` (items 5–7: hass-hubc2000pp = model; CXEM = vectors; READ_ONLY first)
- `docs/reference/orion-path-decision.md` — **binding**
- `docs/reference/c2000pp-modbus.md` — FC 01/03/04 read map; probe FC **3** @ **46152** qty **2**; **do not** read input **3600** (deletes event)
- `docs/reference/hubc2000pp-entities.md` — Point.type `zone` \| `relay` \| `section`; event families
- `docs/reference/orion-frames.md` + `orion-timings.md` — Path B CRC is **not** this adapter; timings still cap TXN ≤ 2000 ms
- `docs/bolid.md` + `docs/device-adapters.md` + `docs/architecture.md`
- `docs/mesh485-protocol.md` + `docs/bridge.md` — READ_ONLY session **blocks adapter TX**
- `docs/security.md` — no fire-disable; mesh loss must not stop the panel
- `docs/oled-ui.md` + `docs/pinout.md`
- `firmware/heltec_v4/include/device/registry.h` + `src/device/registry.cpp`
- `firmware/heltec_v4/include/mesh485/dispatcher.h` + `dispatcher.cpp`
- `firmware/heltec_v4/include/hal/rs485.h` (`writeFrame` / `readFrame` gap)
- `firmware/heltec_v4/include/protocols/bolid/crc8.h` — **leave unwired** to dispatcher / this adapter
- `firmware/heltec_v4/test/native/test_mesh485.cpp` + `test_orion_crc.cpp`
- Official PDF cited in `c2000pp-modbus.md` (АЦДР.426469.020 РЭп Изм.17) — confirm register numbers; do not guess new ones

## Rules

- GPIO **19/20 = USB PHY**. Do not change the Access GPIO map. Do not move RS-485 off 39/38/26.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`. Do **not** define `WITH_RS232_BRIDGE` / `WITH_ESPNOW_BRIDGE`.
- Access OLED stays home. Emergency still wins. 10 s blank unchanged unless a bridge session is open.
- **Never** `UART byte → one LoRa/Companion packet`. Adapter-owned **whole frames** + timeout (same HAL as `RS485_TRANSACTION`).
- Photos never in MESH 485.
- **No live CONTROL** on a fire/security loop. No write coils / FC 05/06/0F/10. No Orion CONTROL opcodes. `COMMAND_EXECUTE` stays `NO_PERMISSION`.
- Do not paste decrypted vendor firmware or GPL Modbus gateway code (zivillian etc. = ideas only).
- Do **not** copy `vendor/hass-hubc2000pp` Python/JS into `firmware/`.
- Do **not** register `BOLID_SIGNAL10` / `BOLID_SIGNAL20` / native Orion UART parser. Do **not** hook `crc8_orion` into `RS485_TRANSACTION` (opaque RAW stays; Modbus uses **CRC-16** on the RTU ADU).
- Do **not** AUTO-discover. No NVS protocol dropdown. `CONFIG_GET` `"protocol"` may stay `"RAW"` (port codec) while the Device record says `BOLID_C2000PP`.
- USB mux unchanged. BLE NUS: do not mux JSON this run.
- Isolated A/B still required on a live bus. Empty bus: **no** fake Bolid events, **no** synthetic type 36, **no** zone FIRE.
- Prefer fixed-size arrays (registry cap 8 adapters / 16 points / 8 events — raise points **modestly** if you must map a handful of zones, not 512).
- `poll()` non-blocking. No 2 s waits. Do not TX every `loop()` — interval on the order of **seconds**, timeout **900** ms (`MESH485_TXN_TIMEOUT_DEFAULT_MS`), cap 2000.
- One cascade file. Stop when the deliverables below exist.
- On Windows, if `pio` is not on PATH: `py -3.13 -m platformio run -e …`. Do **not** run two `heltec_v4_access` builds in parallel (`.pio` race).

## Semantics (must match)

### Device record

Suggested ids (document if you rename):

| Field | Value |
|---|---|
| `id` | `PP-001` |
| `vendor` | `bolid` |
| `model` | `C2000PP` |
| `protocol` | `BOLID_C2000PP` |
| `address` | Modbus slave **1** unless you take it from payload/config RAM (no NVS required) |
| `online` | **true** only after a successful **read** reply with valid Modbus CRC-16 |
| `capabilities` | `["READ_STATUS"]` only (add `READ_ZONES` if you actually map zone Points). **No** `CONTROL_RELAY`. |

Keep `RAW-001` and `TS-001`. `DEVICE_DISCOVER` returns **three** devices. `NODE_STATUS.devices` = 3.

Unknown `device_id` still `UNKNOWN_CMD`.

### Probe (empty-bus stub)

First (and, on this bench, typically only) transaction:

- Modbus-RTU **FC 0x03**, start register **46152** (`0xB448`), quantity **2** (type + version).
- Slave address **1** (common default; if you parameterize, keep it in RAM).
- Timeout **900** ms. Build a correct RTU ADU (addr, FC, start, qty, **CRC-16 Modbus**, low byte first).
- Empty bus / no CRC-valid reply → `online = false`. Points may be empty **or** one diagnostic point `PP` type `loop` state `OFFLINE` / `FAULT`. `EVENT_LIST` for this device stays `[]`.
- **Do not** synthesize holding value 36. **Do not** poll input register 3600.

If a converter answers: `online = true`; optionally read max zones/relays/sections at 46145–46146 (FC3) then a **small** slice of state (e.g. zone 1 @ 40000, section 1 @ 44096, coil 10000) — not 512 registers in one `poll()`. Map codes via `hubc2000pp-entities.md` families to `Point.state` (`NORMAL` / `FAULT` / `ALARM` / `ARMED` / `DISARMED` / `UNKNOWN`). Do not expose Modbus FC in JSON.

### Path A bus master

Heltec is Modbus **client**. С2000-ПП is **slave**. Orion bus master stays Bolid. Default UART **9600 8N1** until the converter’s host settings are known (`c2000pp-modbus.md` table 1.1.2.5).

### Bridge interaction

If a `READ_ONLY` or `CONTROL` session is open: this adapter **must not** `writeFrame` (same rule as Phase 08). Probe only in MONITOR (no session) or during an explicit `RAW_BRIDGE` if you reuse the port — prefer **skip poll** entirely while any bridge session is open so READ_ONLY stays the fire line.

Access GPIO path is independent of RS-485.

### OLED

Access READY remains home. Add a **stub** line or short overlay: device id `PP-001` plus `NORMAL` / `FAULT` / `ALARM` (offline → `FAULT`). Do not add MeshCore chat, BLE PIN, or a new protocol-debug screen. USER / 10 s blank / `BRIDGE` abort unchanged. Document in `docs/oled-ui.md` if the READY row changes.

### Cooperative loop

`poll()`: at most one in-flight frame; use HAL `writeFrame` + `readFrame`; parse CRC-16; return quickly. Do not starve `mesh.loop()`.

## Deliverables

### 1. Spec

- Update `docs/device-adapters.md`: third adapter, ids, empty-bus behaviour, READ_ONLY no-TX, capabilities.
- Update `docs/mesh485-protocol.md`: `DEVICE_DISCOVER` lists three devices; `CONFIG_GET` protocol still `RAW` unless you document a one-line exception.
- Golden examples: `protocol/mesh485/examples/` for DISCOVER (three ids) and `DEVICE_INFO` `PP-001` (`online: false` is valid).
- Keep `docs/reference/c2000pp-modbus.md` true (cite pages you actually coded).

### 2. Firmware

Suggested tree (names can vary):

```
include/protocols/modbus/crc16.h          ← RTU CRC-16 only
src/protocols/modbus/crc16.cpp
include/protocols/bolid/c2000pp_modbus_adapter.h
src/protocols/bolid/c2000pp_modbus_adapter.cpp
```

- Register in `DeviceRegistry` (third slot). Bind the same `Mesh485Port` as RAW.
- Implement Modbus CRC-16 (poly 0xA001 / 0x8005 reflected — standard RTU). **Do not** use `crc8_orion` on these frames.
- No `src/protocols/bolid/c2000/` Orion UART parser. No Signal-10/20.
- Do not change Access GPIO, USB mux, or Phase 09 CRC files except to leave them unwired.

### 3. Tests

Extend `[env:native_protocol_tests]`:

- `DEVICE_DISCOVER` contains `RAW-001`, `TS-001`, and `PP-001` (or your documented id)
- Empty / loopback-without-Modbus-reply: `PP-001` `online: false`; `EVENT_LIST` not a fake fire event
- Optional: inject a valid FC03 response (type 36, version n) → `online: true` (native fake port)
- Flipped Modbus CRC → still offline / not mapped as a Point
- `COMMAND_EXECUTE` still `NO_PERMISSION`
- `BRIDGE_OPEN` READ_ONLY: adapter does **not** TX (txCount unchanged)
- Phase 06–09 cases still pass (`crc8_orion` fixtures, DISCOVER still includes the two stubs)

`pio run -e native_protocol_tests` (or `py -3.13 -m platformio run -e native_protocol_tests`) must succeed; run `program.exe` and expect `native_protocol_tests: ok`.

### 4. On-device

- `pio run -e heltec_v4_access` must **build**. This run **changes the image** — flash **COM23** when free (close serial monitor; wait for CDC after 1200 bps). If upload fails with PermissionError, say so; do not fake a device ACK.
- USB mux: `python firmware/heltec_v4/scripts/m485_devices.py COM23 discover` (extend the script if it assumes two devices). Expect three ids. Empty bus: `PP-001` offline, `EVENT_LIST` `[]`.
- `{` still muxed; Companion boot lines may still reset the S3 on re-open.
- Isolated transceiver still required on a live converter. Do **not** send write coils / Orion CONTROL on A/B.

### 5. Docs / cascade

- `docs/implementation.md` Phase 10.
- `firmware/heltec_v4/README.md` — one paragraph: DISCOVER includes `PP-001`; read-only; empty bus TIMEOUT.
- `AGENTS.md` / `agent/CASCADE.md` → next file `11-events-queue.md`.
- Cursor rules `Next agent file` → `11-events-queue.md` if you touch them.

## Forbidden this run

- Path B native Orion UART adapter; wiring `crc8_orion` into the mux / `RS485_TRANSACTION`.
- Signal-10 / Signal-20 (prompts 15–16).
- Write coils, preset registers, `arm`/`disarm`/`relay_on` (HUB IP API is not ESP32).
- Reading Modbus input **3600** (destructive).
- `COMMAND_EXECUTE`; fire silence; CONTROL capability.
- Fake Bolid events on empty A/B; registering only `PP-001` and dropping the Access/RAW stubs.
- Copying hass-hubc2000pp or GPL gateways into firmware.
- Flutter, FastAPI, photos, OTA, Excel, AUTO discovery, NVS protocol persist.
- MeshCore chat UI; editing `vendor/MeshCore`; enabling RS232/ESP-NOW bridge.
- Changing Access GPIO map; byte streaming RAW over LoRa.
- `ACCESS_*` 3MAP (prompt 17). Event mesh publish / dual ACK (prompt 11).
- Pasting decrypted Bolid firmware.

## Done when

1. Third adapter registered; DISCOVER has RAW + Access + С2000-ПП ids.
2. Empty bus: `online: false`, no invented zones/events; probe is a **read**.
3. No write FCs; `COMMAND_EXECUTE` refused; READ_ONLY session still blocks adapter TX.
4. Native tests include 06–09 plus the new cases; `heltec_v4_access` builds (flash COM23 if the port is free).
5. Phase 10 note in `docs/implementation.md`. OLED stub documented if READY changed.

Then **stop**. Next cascade file: `agent/prompts/11-events-queue.md`.
