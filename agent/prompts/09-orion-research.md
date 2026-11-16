# PROMPT 09 — Orion / Bolid research freeze (docs + CRC tests, little firmware)

You are implementing **MESH 485 Extender** in this repo. This is **run 9 of 19**. Do **only** this run. Stop when Orion **frame layout**, a **verified CRC-8**, a **timing table**, **hass-hubc2000pp entity names**, and a **Path A vs Path B decision for prompt 10** are frozen in `docs/reference/` plus native CRC tests — **not** a Bolid `DeviceAdapter`, **not** С2000-ПП Modbus polling, **not** Signal-10/20, **not** live CONTROL on a fire loop, **not** Flutter, **not** `COMMAND_EXECUTE`.

Phases **01–08 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + `RS485_TRANSACTION` + USB mux, `BRIDGE_*` default **READ_ONLY**, `DeviceRegistry` with stubs `RAW-001` / `TS-001`).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile + DeviceAdapter registry.

```
MeshCore → MESH 485 Protocol → DeviceRegistry
         → RawRs485Adapter | AccessTurnstileAdapter   ← already shipping
         → (this run: CRC + docs only)
         → Bolid adapter (prompt 10) → Rs485Hal
```

Clients must still see **Device / Point / Event**, never “Bolid command 0x17” on MESH 485. This run is **research freeze**: legal sources + test vectors + a CRC function behind tests. Prompt 10 is the first **read-only** adapter. Prompt 11 is event queue / dual ACK. Prompt 14 is capability CONTROL.

## Read first

- `AGENTS.md` (items 5–7: hass-hubc2000pp = model only; CXEM = vectors; READ_ONLY first)
- `docs/architecture.md` — Path A (С2000-ПП Modbus) vs Path B (native Orion); dual ACK is later
- `docs/bolid.md` — current draft; **keep this file true** after the freeze
- `docs/device-adapters.md` — do **not** register `BOLID_*` this run
- `docs/mesh485-protocol.md` + `docs/bridge.md` — `RS485_TRANSACTION` cap 2000 ms; `RAW_BRIDGE` is lab-only
- `docs/security.md` + `SECURITY.md` — no fire-disable op; Mesh loss must not stop the panel
- `docs/vendor-map.md` + `NOTICE.md` + `docs/THIRD_PARTY.md`
- `docs/pinout.md` + `hardware/rs485-adapter.md` (UART1 TX39 / RX38 / DE26, isolated PHY)
- `firmware/heltec_v4/include/hal/rs485.h` + `src/hal/rs485.cpp` (`writeFrame` / `readFrame` gap)
- `firmware/heltec_v4/include/mesh485/types.h` (`MESH485_TXN_TIMEOUT_MAX_MS` 2000, default 900)
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` — Phase 08 ops; `COMMAND_EXECUTE` still `NO_PERMISSION`
- `tools/clone-vendor.ps1` — clone `hass-hubc2000pp` if `vendor/hass-hubc2000pp` is missing
- Official С2000-ПП / Orion interface PDFs you can **legally** use (bolid.ru manuals)
- [hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) README (TCP/UDP to **HUB-C2000PP** — not UART)
- CXEM: [Протокол Орион (Болид)](https://forum.cxem.net/topic/105126-%D0%BF%D1%80%D0%BE%D1%82%D0%BE%D0%BA%D0%BE%D0%BB-%D0%BE%D1%80%D0%B8%D0%BE%D0%BD-%D0%B1%D0%BE%D0%BB%D0%B8%D0%B4/) — dumps and CRC discussion as **test vectors only**

## Rules

- GPIO **19/20 = USB PHY**. Do not change the Access GPIO map. Do not move RS-485 off 39/38/26.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`. Do **not** define `WITH_RS232_BRIDGE` / `WITH_ESPNOW_BRIDGE`.
- Keep Access OLED as home. No MeshCore chat, no BLE PIN, no photos. No new OLED protocol screens this run.
- **Never** `UART byte → one LoRa/Companion packet`. CRC lives in a native-tested module; it is **not** a Companion codec.
- Photos never in MESH 485.
- **No live CONTROL** onto a building fire/security loop. No `COMMAND_EXECUTE`. No write coils / Orion control opcodes on A/B.
- Do not paste **decrypted vendor firmware** (CXEM or elsewhere) into this repo. Hex **dumps of bus frames** with a source URL and date are OK as fixtures.
- `hass-hubc2000pp` is Apache-2.0 **entity/event mapping**. It talks to HUB-C2000PP over **IP**. Do **not** copy the HA component or its TCP stack into ESP32 firmware. Do **not** treat it as an Orion UART parser.
- GPL Modbus gateways (zivillian, etc.): ideas only — do not paste their code.
- Do **not** register `BOLID_C2000` / `BOLID_C2000PP` / `BOLID_SIGNAL10` / `MODBUS_RTU` in `DeviceRegistry` this run. Stubs stay `RAW-001` + `TS-001`.
- Do **not** hook Orion CRC into `RS485_TRANSACTION` yet (still opaque RAW / gap-framed). Dispatcher `error: CRC` stays unused until prompt 10.
- USB mux unchanged. BLE NUS: do not mux JSON this run.
- Native CRC tests: no Arduino HAL. Windows MSVC `g++` shim is OK.
- Isolated A/B still required on a live bus. Empty bus: do not invent instrument ACKs or Bolid events.
- One cascade file. Stop when the deliverables below exist.

## Semantics (must match)

### Two legal paths (architecture)

**Path A (preferred when hardware exists):**

```
Orion devices  ↔  С2000-ПП (official Modbus RTU)  ↔  isolated RS-485  ↔  Heltec
```

Bus master stays Bolid. Heltec is a Modbus **client** (read-only in prompt 10). Zone / relay / section counts and register map come from the **current official PDF**, not from guessing. hass-hubc2000pp names map onto those points later.

**Path B (native Orion):**

Heltec is a protocol-aware peer on Orion RS-485 (typical **9600 8N1** — confirm per device PDF). Frames + CRC-8 + timeouts from official docs **and** verified dumps. MONITOR first. CONTROL only in prompt 14 via capabilities.

This run **does not ship either adapter**. It freezes enough that prompt 10 can pick one path without re-litigating CRC or frame layout.

### Orion frame (Path B) — freeze in docs

Write `docs/reference/orion-frames.md` so an implementer can parse a capture without the forum:

| Field | Where in the byte string | Notes |
|---|---|---|
| Address | … | device / network address; range from dumps + manuals |
| Length | … | what it counts (payload only vs whole frame) |
| Command / code | … | **document codes seen in dumps**; do not implement a command table that executes them |
| Payload | … | |
| CRC | **placement** (last byte? two bytes?) | polynomial, init, xor-out, reflected or not |

Include **worked examples**: hex dump → expected CRC → pass/fail for the chosen algorithm. Cite source (CXEM post date / PDF name / page). Mark anything unverified as `UNCONFIRMED`.

A correct CRC means the **frame was intact**, not that the panel accepted the function (relay program, section, permissions). Dual ACK (mesh vs device) is prompt 11.

### CRC

`docs/bolid.md` already forbids hard-coding “Dallas/Maxim” until dumps match. Implement behind tests:

```cpp
enum class Crc8Algo : uint8_t {
  None = 0,
  Orion = 1,        // the one dumps matched — production default
  DallasMaxim = 2,  // compare only
  Generic = 3       // compare only
};

uint8_t crc8(Crc8Algo algo, const uint8_t* data, size_t len);
```

- Compare Dallas/Maxim (poly 0x31 reflected / 0x8C variants as needed) **and** any table from dumps **and** a naive “generic” CRC-8 if the thread uses one.
- **Write which algorithm matched** the fixtures (file + one paragraph in `docs/bolid.md` / `orion-frames.md`).
- Production helper: `crc8_orion(...)` calls **only** the verified algo. Keep the others for tests so a future dump can fail loudly.
- Cover: empty, 1 byte, known-good frames from fixtures, known-bad (flipped bit → mismatch).
- Do **not** TX these frames on COM23 / UART1 this run.

### Timing table

Write `docs/reference/orion-timings.md` (or a section in `orion-frames.md`). Design bounds from Bolid radio-modem notes and `docs/bolid.md` — label them **design bounds, not gospel**:

| Event | Bound (ms) | Implication for this node |
|---|---|---|
| Direction change / DE | ~50 | HAL already holds DE after TX; do not add 2 s waits |
| Event reply | ~800 | `RS485_TRANSACTION` default 900 is in-band |
| Command | ~900 | cap stays ≤ 2000 (`MESH485_TXN_TIMEOUT_MAX_MS`) |
| Search / scan | ~600 | AUTO discovery is later; do not scan this run |

**Why `RAW_BRIDGE` is unsafe as the Orion path:** Mesh / Companion round-trip is not a 50 ms UART turnaround. Byte-streaming a fire bus through LoRa or USB Companion **desynchronizes** the panel. Permanent path = whole-frame `RS485_TRANSACTION` (and, in prompt 10, adapter-owned transactions). `READ_ONLY` bridge captures RX only — keep that as the safety line.

Relate numbers to existing HAL: `readFrame` stops on **inter-byte gap** after first RX byte, else `timeout_ms`. Document a recommended gap vs Orion inter-byte (if dumps/PDF say; else `UNCONFIRMED` + keep current HAL).

### hass-hubc2000pp entities (model only)

If `vendor/hass-hubc2000pp` is missing: `.\tools\clone-vendor.ps1` from repo root (skips existing clones; clones this one).

Extract into `docs/reference/hubc2000pp-entities.md` (names, not Python):

- Zone / relay / section (partition) — ids, typical states
- Event types / severity-like labels used by the HA component
- Poll vs UDP push (HUB TCP 22000 PING/PONG, UDP on +1) — **server-side later**, not ESP32
- Mapping sketch: HA entity → MESH 485 `Point.type` (`zone` \| `relay` \| `loop` \| `section`) and `Event.type`

Do **not** copy YAML/Python into `firmware/`. Apache-2.0 attribution stays in `NOTICE.md` (already listed).

### Path decision for prompt 10

Write `docs/reference/orion-path-decision.md` (short, dated). Pick **one** for the next run, based on **hardware on this bench** (or explicit “none — lab USB-RS485 only”):

| If | Prompt 10 does |
|---|---|
| С2000-ПП (or equivalent official converter) on the isolated bus | **Path A**: `bolid/c2000pp_modbus`, read-only register poll, map to Points |
| No converter, but one native panel (C2000 **or** Signal-20) | **Path B**: one MONITOR adapter, listen/poll events only, CRC from this freeze |
| Neither (empty A/B, dongle only) | Still prefer documenting Path A as the **product default**; prompt 10 may ship a **Modbus RTU read stub** that times out cleanly (`device_ok: false`) **or** a native MONITOR that never TX — **say which** and why |

Do not leave “TBD”. Prompt 10 will follow this file.

### С2000-ПП Modbus sketch (docs only)

If an official PDF is available: freeze a **minimal read map** in `docs/reference/c2000pp-modbus.md` — baud/parity on the host side as specified, function codes for **read** (holding/input), example zone/relay/section register ranges **from the PDF** (cite page). No write-coil table this run. If no PDF in tree / not legally fetchable: say so and list the exact manual name to obtain; do not invent registers.

### Registry / protocol (do not implement)

Keep `CONFIG_GET` `"protocol":"RAW"`. Do not add NVS dropdown. Do not AUTO-discover Bolid devices. `DEVICE_DISCOVER` still returns only `RAW-001` and `TS-001`.

## Deliverables

### 1. Spec freeze

- `docs/reference/orion-frames.md` — layout + CRC placement + worked hex
- `docs/reference/orion-timings.md` — or a timings section; include why RAW_BRIDGE is unsafe
- `docs/reference/hubc2000pp-entities.md` — entity names from the clone
- `docs/reference/orion-path-decision.md` — Path A vs B for prompt 10
- Optional: `docs/reference/c2000pp-modbus.md` if PDF-backed
- Fixtures: `docs/reference/orion-vectors/` (or `firmware/heltec_v4/test/native/orion_vectors.h`) — hex + expected CRC + source URL. **No decrypted firmware binaries.**
- Update `docs/bolid.md` to match (CRC algo that matched; timings; pointer to the decision file)
- Update `docs/vendor-map.md` if the clone path is now real

### 2. Firmware (small)

Suggested tree (names can vary):

```
include/protocols/bolid/crc8.h
src/protocols/bolid/crc8.cpp
```

- Enum + `crc8_orion` used by tests. **Not** wired to dispatcher, adapters, or UART TX.
- Do not add `src/protocols/bolid/c2000/` parsers beyond this CRC file.
- Do not change Access GPIO, USB mux, or Phase 08 registry.

### 3. Tests

Extend `[env:native_protocol_tests]` (or a sibling `native_orion_crc` env that still uses the MSVC shim):

- Known-good dump frames → `crc8_orion` matches fixture
- Flipped bit → mismatch
- Dallas/Maxim (and generic, if you compared it) documented as match **or** mismatch vs the same fixtures
- Existing Phase 06/07/08 cases still pass (`DEVICE_DISCOVER` still `RAW-001`+`TS-001`, `COMMAND_EXECUTE` still `NO_PERMISSION`, READ_ONLY still blocks TX)

`pio run -e native_protocol_tests` must succeed.

### 4. On-device

- `pio run -e heltec_v4_access` still **builds**. Flash COM23 **only if** you touched the device image; CRC-only native code does not require a flash.
- If you flash: USB mux still `{` → dispatcher; `DEVICE_DISCOVER` still two stubs; empty bus still no fake Bolid events.
- Do **not** send Orion CONTROL / write coils on A/B. Isolated transceiver still required on a live panel bus.

### 5. Docs / cascade

- `docs/implementation.md` Phase 09.
- `firmware/heltec_v4/README.md` — one line: Orion CRC is unit-tested; not on the mux yet.
- `AGENTS.md` / `agent/CASCADE.md` → next file `10-bolid-readonly.md`.

## Forbidden this run

- Bolid `DeviceAdapter` / Modbus poller / Signal-10 / Signal-20 (prompts 10, 15–16).
- Native Orion CONTROL packets; fire silence; `COMMAND_EXECUTE`; write coils.
- Registering fake `BOLID_*` devices on `DEVICE_DISCOVER`.
- Pasting decrypted vendor firmware or GPL gateway parsers.
- Copying hass-hubc2000pp into ESP32 as a UART stack.
- Wiring CRC into `RS485_TRANSACTION` (opaque RAW stays).
- Flutter, FastAPI, photos, OTA, Excel, AUTO discovery, NVS protocol persist.
- MeshCore chat UI; editing `vendor/MeshCore`; enabling RS232/ESP-NOW bridge.
- Changing Access GPIO map; byte streaming RAW over LoRa.
- `ACCESS_*` 3MAP (prompt 17). Event mesh publish (prompt 11).

## Done when

1. `orion-frames.md` has address/length/command/CRC placement and at least one worked hex example with expected CRC.
2. Native tests prove **which** CRC-8 matched the fixtures; production `crc8_orion` is that one only.
3. Timing table exists and states why `RAW_BRIDGE` is not the Orion path.
4. `vendor/hass-hubc2000pp` cloned if it was missing; entity names extracted (not the HA component).
5. `orion-path-decision.md` picks Path A or Path B (or a documented empty-bus fallback) for prompt 10.
6. Phase 08 mux behaviour unchanged; `COMMAND_EXECUTE` still refused; no CONTROL on the wire.
7. Phase 09 note in `docs/implementation.md`.

Then **stop**. Next cascade file: `agent/prompts/10-bolid-readonly.md`.
