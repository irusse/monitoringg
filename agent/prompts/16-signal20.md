# PROMPT 16 — Signal-20 adapter (MONITOR first)

You are implementing **MESH 485 Extender** in this repo. This is **run 16 of 19**. Do **only** this run. Stop when a **Signal-20** `DeviceAdapter` is registered beside Signal-10 / С2000-ПП, polls (or probes) MONITOR/read of events and zones, times out cleanly on an empty bus, `CONFIG_SET` can select `BOLID_SIGNAL20` **manually** (never AUTO), native + `heltec_v4_access` still pass — **not** a second С2000-ПП, **not** native C2000 UART panel, **not** AUTO discovery, **not** ACL 3MAP / OPEN, **not** fire-disable.

Phases **01–15 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + `RS485_TRANSACTION` + USB mux, `BRIDGE_*` default **READ_ONLY**, `DeviceRegistry` stubs `RAW-001` / `TS-001` / Path A `PP-001` / Path B `S10-001`, Orion CRC-8/MAXIM, event queue / dual ACK, Flutter console + RU UI, capability-gated `CONTROL_RELAY` on PP-001 only, Signal-10 MONITOR + RAM `CONFIG_SET` protocol).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile + DeviceAdapter registry.

```
MeshCore → MESH 485 Protocol → DeviceRegistry
         → RawRs485Adapter | AccessTurnstileAdapter | C2000ppModbusAdapter
         → Signal10Adapter
         → Signal20Adapter (this run)
         → Rs485Hal
```

Clients must see **Device / Point / Event**, never “Bolid command 0x17”. Prompt 10 was **С2000-ПП Modbus**, **not** a native Signal-20 adapter — **do not extend PP-001**; add a new `bolid/signal20/` adapter. Dual ACK already exists. Capability model is prompt 14.

## Frozen decisions (do not re-litigate)

- Path A default remains `PP-001` Modbus probe when CONFIG `protocol` is `RAW` or `BOLID_C2000PP`.
- Path B Orion CRC = `crc8_orion` (CRC-8/MAXIM). Already used by Signal-10. **Still not** on `RS485_TRANSACTION` (opaque RAW).
- One isolated UART. **Only one Bolid adapter may `writeFrame` at a time** (CONFIG protocol selects).
- **No AUTO** discovery / bus scan. `CONFIG_SET` `protocol` `AUTO` stays `UNKNOWN_CMD` / `no AUTO`.
- Empty A/B: **no** fake FIRE, **no** synthetic type 36, **no** invented zone ALARM.
- Prompt 10 was **not** Signal-20 native. Do not “upgrade” `PP-001`.

## Read first

- `AGENTS.md` (items 5–7: hass-hubc2000pp = model; CXEM = vectors; MONITOR first)
- `docs/reference/signal10-orion.md` — **copy this pattern** (probe, empty-bus, no invented zones)
- `docs/reference/orion-frames.md` + `orion-timings.md` + `orion-vectors/`
- `docs/bolid.md` — CXEM row **“Signal-20 relay examples”**: CRC OK ≠ panel accepted (relay **program**, section, permissions)
- `docs/device-adapters.md` + `docs/mesh485-protocol.md` + `docs/security.md`
- `firmware/heltec_v4/include/protocols/bolid/signal10/signal10_adapter.h` + `.cpp` — template
- `firmware/heltec_v4/include/device/registry.h` + `src/device/registry.cpp` (`setBusProtocol`)
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` (`CONFIG_GET` `protocols` array, `CONFIG_SET`)
- `firmware/heltec_v4/test/native/test_mesh485.cpp` (phase 15 cases — clone for S20)
- Official Сигнал-20 / Сигнал-20П РЭ (zone/relay counts) — product facts only; **do not guess opcodes**

## Rules

- GPIO **19/20 = USB PHY**. Do not change the Access GPIO map. Do not move RS-485 off 39/38/26.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`.
- Access OLED stays home. Overlay: when CONFIG protocol is `BOLID_SIGNAL20`, READY line `S20-001 FAULT` / `NORMAL` (same 20-char slot as PP/S10). Emergency / USER / 10 s blank / `BRIDGE` abort unchanged.
- **Never** `UART byte → one LoRa/Companion packet`. Adapter-owned whole frames + timeout.
- Photos never in MESH 485.
- Prefer sharing Orion frame/CRC helpers with Signal-10 (extract a tiny `orion_frame` if copy-paste would duplicate XOR+CRC). Do **not** duplicate a second CRC polynomial.
- Do **not** copy `vendor/hass-hubc2000pp` into `firmware/`. Do **not** paste decrypted Bolid firmware.
- `poll()` non-blocking. Interval ~**5 s**, timeout **900** ms, cap 2000. At most one in-flight frame. Do not starve `mesh.loop()`.
- One cascade file. Stop when the deliverables below exist.
- On Windows: `py -3.13 -m platformio run -e …`. Do **not** run two `heltec_v4_access` builds in parallel.

## Semantics (must match)

### Device record

| Field | Value |
|---|---|
| `id` | `S20-001` |
| `vendor` | `bolid` |
| `model` | `SIGNAL20` |
| `protocol` | `BOLID_SIGNAL20` |
| `address` | Orion **1** unless RAM-parameterized (no NVS) |
| `online` | **true** only after a CRC-valid **read** reply (`crc8_orion`) |
| `capabilities` | `["READ_STATUS"]`; add `READ_ZONES` if a zone Point is actually mapped |

Keep `RAW-001`, `TS-001`, `PP-001`, `S10-001`. `DEVICE_DISCOVER` returns **five** devices. `NODE_STATUS.devices` = **5**.

Unknown `device_id` still `UNKNOWN_CMD`.

`protocol[16]` fits `BOLID_SIGNAL20` (14 chars).

### MONITOR probe

Same family as Signal-10 unless a **Signal-20-specific dump** in `orion-vectors/` or a cited published hex string justifies a different status/event read:

- Default: reuse the Habr encrypted **status request** (`0x57` / `0x02`, `MESSAGE_KEY 0xBA`, `crc8_orion`) parameterized by address — document in `docs/reference/signal20-orion.md`.
- If you find a **published** Signal-20 event/zone read frame, prefer that for MONITOR and cite it. Do not invent opcodes.
- Timeout **900** ms. Empty bus / flipped CRC → `online: false`. Points: diagnostic `S20` type `loop` state `FAULT`. `EVENT_LIST` for this device stays `[]` (or only `DIAG` after a CRC-valid read, `device_ok: true`).
- **Do not** map 20 FIRE zones. After a valid reply: at most a **modest** slice (`Z1` `UNKNOWN` is OK — same honesty as S10). Product may have ~20 ШС; dumps still have no bitmap unless you cite one.

### CONTROL (only if dumps justify)

Prompt 14 capabilities **exist** (`CONTROL_RELAY`, named `R1`…`R8`, OPERATOR+, extra UI confirm, no fire-disable).

This run may advertise `CONTROL_RELAY` on `S20-001` **only if**:

1. A **published dump or official doc** gives a native Orion relay command (CXEM Signal-20 relay examples). Cite frame hex in `docs/reference/signal20-orion.md` and native tests.
2. The adapter maps a **named** point (`R1`…), not “all relays”.
3. Panel allow-list is **empty by default** (same as PP-001). Lab: `allowRelay(n)` or a compile flag.

**Mandatory caveat (the point of this prompt):** CXEM / `docs/bolid.md`: CRC OK on the wire does **not** mean the relay clicked. Result depends on **relay program** in the panel (and section / permissions). Encode that:

- `device_ok: true` = CRC-valid **echo / accepted frame**, **not** “contact closed”.
- `DIAGNOSTICS` and/or point `attributes` / docs: relay program may still block actuation.
- Do **not** synthesize a click. Empty bus after an allowed TX → `TIMEOUT` (`mesh_ok: true`, `device_ok: false`).

If **no** dump justifies a relay opcode: **do not** advertise `CONTROL_RELAY` on S20-001; `execute()` stays `NO_PERMISSION` (same as Signal-10). Write-key `0x11` is **not** a product capability.

Never `DISABLE_FIRE_ALARM` / `DISARM_ALL` / silence-all.

### CONFIG (manual protocol select)

Extend RAM allow-list (still **no NVS**, **no AUTO**):

`RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10` | `BOLID_SIGNAL20`

TX owner:

| `CONFIG` protocol | Who may `writeFrame` |
|---|---|
| `RAW` or `BOLID_C2000PP` (default `RAW`) | `PP-001` only |
| `BOLID_SIGNAL10` | `S10-001` only |
| `BOLID_SIGNAL20` | `S20-001` only |

`CONFIG_GET.protocols` must list the four keys above. Update golden `config_set.response.json`. Viewer `CONFIG_SET` still `NO_PERMISSION`. Baud persist still refused.

S20 `poll()` / `readFrame` **only** when selected (do not steal PP/S10 replies).

### Bridge

Any open `READ_ONLY` / `CONTROL` session: this adapter **must not** `writeFrame`. Skip probe (same as S10/PP).

### Cooperative loop

`poll()`: at most one in-flight frame; HAL `writeFrame` + `readFrame`; parse CRC-8; return quickly.

## Deliverables

### 1. Spec

- `docs/reference/signal20-orion.md` — probe, empty-bus, CONTROL yes/no + dump citations + relay-program caveat.
- Update `docs/device-adapters.md`, `docs/mesh485-protocol.md`, `docs/bolid.md`, `docs/security.md`, `docs/oled-ui.md`.
- Golden: `device_discover` five ids; `device_info_s20001.response.json` (`online: false` valid).

### 2. Firmware

```
include/protocols/bolid/signal20/signal20_adapter.h
src/protocols/bolid/signal20/signal20_adapter.cpp
```

Register fifth slot in `DeviceRegistry`. Bind the same `Mesh485Port`. Extend `setBusProtocol`. OLED overlay when selected.

### 3. Tests (`native_protocol_tests`)

Keep 06–15. Add:

- DISCOVER includes `S20-001` / `BOLID_SIGNAL20`; `devices`: **5**
- Dump/status req CRC; flipped CRC → offline
- Empty bus after `CONFIG_SET` `BOLID_SIGNAL20`: `online: false`, no FIRE, diagnostic `S20`, no fake zones
- Valid CRC reply (native fake port) → `online: true`; no invented FIRE
- Default `RAW`: S20 does **not** TX (PP still 8-byte Modbus)
- `CONFIG_SET` `AUTO` still refused; `BOLID_SIGNAL20` accepted (ADMIN)
- `BRIDGE_OPEN` READ_ONLY: S20 does not TX
- If no CONTROL: `COMMAND_EXECUTE` `CONTROL_RELAY` on `S20-001` → `NO_PERMISSION`
- If CONTROL wired: named `R1` + allow-list; empty bus `TIMEOUT`; dual ACK; **test must not treat CRC-only as “clicked”** beyond `device_ok` meaning frame echo; forbidden caps still refused

`py -3.13 -m platformio run -e native_protocol_tests` then `program.exe` → `native_protocol_tests: ok`.

### 4. On-device

- `pio run -e heltec_v4_access` must **build**. Flash **COM23** when free.
- USB mux: `python scripts/m485_devices.py COM23 all` — five ids; empty bus S20 offline; `CONFIG_GET` lists `BOLID_SIGNAL20`; `CONFIG_SET AUTO` still `no AUTO`.
- Isolated A/B still required on a live panel.

### 5. App / cascade

- Flutter fixture + Objects note: `S20-001`. Widget test expects the id.
- `docs/implementation.md` Phase 16. `AGENTS.md` / `CASCADE.md` / Cursor `Next agent file` → `17-access-acl.md`.

## Forbidden this run

- AUTO discovery / NVS protocol persist / bus address scan.
- Treating CRC-valid relay TX as physical click.
- Fire silence / `DISARM_ALL` / generic disable-alarm.
- Native C2000 (прибор) UART adapter; wiring `crc8_orion` into `RS485_TRANSACTION`.
- Dropping RAW / Access / PP / S10 from DISCOVER.
- Fake Bolid events on empty A/B.
- Copying hass-hubc2000pp or GPL gateways into firmware.
- Access `ACCESS_*` / 3MAP / OPEN (prompt 17–18).
- MeshCore chat UI; editing `vendor/MeshCore`.
- Changing Access GPIO map; byte streaming RAW over LoRa.

## Done when

1. Fifth adapter registered; DISCOVER has RAW + Access + PP + S10 + S20.
2. Empty bus: `S20-001` `online: false`, no invented FIRE/zones; probe is a **read**.
3. CONFIG lists `BOLID_SIGNAL20`; AUTO still refused; only one Bolid TX owner.
4. CONTROL only if dumps justify; relay-program caveat encoded; otherwise `NO_PERMISSION`.
5. Native tests include 06–15 plus new cases; `heltec_v4_access` builds (flash COM23 if free).
6. Phase 16 note in `docs/implementation.md`.

Then **stop**. Next cascade file: `agent/prompts/17-access-acl.md`.
