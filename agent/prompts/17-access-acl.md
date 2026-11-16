PROMPT 17 — Access ACL + 3MAP (PROFILE_ACCESS on MESH 485)

You are implementing **MESH 485 Extender** in this repo. This is **run 17 of 19**. Do **only** this run. Stop when the phase-04 **GRANT-any-UID stub is gone**, Wiegand UIDs are checked against a persistent offline ACL (DENY unknown), ALLOW/DENY drives lamp + OPEN pulse + OLED, card events enqueue with real UUIDs on the existing `EventStore`, and MESH 485 `ACCESS_*` implements 3MAP **STATUS / OPEN / LOCK / GET_EVENTS** (plus ADMIN card add/remove so the ACL can be filled without the Flutter employees UI) — **not** employees CRUD / photos in the app (18), **not** `SECURITY.md` (19), **not** AUTO discovery, **not** Bolid CONTROL, **not** fire-disable, **not** MeshCore chat.

Phases **01–16 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL with stub ACL, isolated RS-485 UART1, MESH 485 JSON + bridge READ_ONLY, DeviceRegistry `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`, Orion CRC + dual ACK + Flutter console + RU UI + capability-gated `CONTROL_RELAY` on PP-001 only, Signal-10/20 MONITOR, RAM `CONFIG_SET` protocol, no AUTO).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + **PROFILE_ACCESS** (Wiegand + ACL + 3MAP) + DeviceAdapter registry.

```
Wiegand 15/16 → AccessController (ACL) → TurnstileHAL + OLED
                              ↓
                     EventStore (SPIFFS) → EVENT_LIST / ACCESS_GET_EVENTS
MESH 485 ACCESS_OPEN / ACCESS_LOCK → same HAL + showRemote
```

3MAP (`docs/protocol-3map.md`) is a **profile namespace inside MESH 485**, not a second mesh. Clients send `op: ACCESS_OPEN` (alias `TURNSTILE_OPEN`). Do **not** invent a parallel `access.command` USB stream. OPEN is **not** `COMMAND_EXECUTE`.

## Frozen decisions (do not re-litigate)

- GPIO **19/20 = USB PHY**. Access map stays `docs/pinout.md` / `include/access/pins.h` (D0/D1 **15/16**, OPEN **40**, lamps 41/33, ENTRY/EXIT/ESTOP 3/4/6). Do not move RS-485 off 39/38/26.
- **SPIFFS**, not LittleFS. Same filesystem as MeshCore `DataStore` and `EventStore` `/m485_evt.bin`. Store ACL as `/m485_acl.bin` (or equivalent). One FS policy.
- Default ACL is **empty → unknown UID = DENY**. Replace phase-04 “any UID = GRANT”. Lab seed: `AccessController::allowUid(hex)` and/or `-D MESH485_ALLOW_TEST_UID=1` (one documented test UID). Do not ship GRANT-all.
- EM-Marine UID is **not** crypto. Do not claim high-security. Cloneable cards are in-scope for convenience access only (full `SECURITY.md` is 19).
- Remote OPEN requires **OPERATOR or ADMIN** even if the operator is already on the mesh. VIEWER → `NO_PERMISSION`. Replay window already exists (30 s / 120 s ts).
- Dual ACK already exists: `mesh_ok` vs `device_ok`. Local Wiegand ALLOW → `device_ok: true`. Remote OPEN that pulsed GPIO 40 → `device_ok: true`. Empty/missing HAL in native tests: still honest (`device_ok` follows the fake HAL, not a synthesized physical click).
- Keep five DISCOVER devices. Do not drop RAW / PP / S10 / S20. Do not TX Bolid frames from this run.
- Photos never on the node, OLED, LoRa, or MESH 485.
- Employees list / photos / Flutter CRUD is **prompt 18**. This run may wire a live **OPEN** button that calls `ACCESS_OPEN` so the op can be verified; do not build the employees module.

## Read first

- `AGENTS.md` (Access = PROFILE_ACCESS on this image; Wiegand from `vendor/esp32-rfid` + `vendor/WiegandNG`, **not** their GPIO numbers)
- `docs/pinout.md` + `docs/oled-ui.md` + `docs/protocol-3map.md` + `docs/mesh485-protocol.md` (Relation to 3MAP)
- `docs/security.md` (remote OPEN needs authz; UID not crypto)
- `firmware/heltec_v4/include/access/access_controller.h` + `.cpp` (GRANT-all stub)
- `firmware/heltec_v4/src/main.cpp` (`reader.poll` → `acl.checkUid` → `handleCard` / `noteCard`)
- `firmware/heltec_v4/include/turnstile/turnstile_hal.h` + Wiegand reader
- `firmware/heltec_v4/include/storage/event_store.h` (reuse; UUID + `source=RFID`)
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` (`startsWith(op, "ACCESS_")` currently refuses all)
- `firmware/heltec_v4/include/protocols/access/access_turnstile_adapter.h`
- `app/mesh485/lib/screens/access_screen.dart` (OPEN button `onPressed: null`)
- Native: `test/native/test_mesh485.cpp` (`ACCESS_OPEN` currently expects `NO_PERMISSION` — **update** that case)

## Rules

- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`.
- Access OLED stays home. GRANT / DENY / REMOTE / EMERGENCY / 10 s blank / USER menu unchanged in meaning. Remote OPEN must call `showRemote` then the existing granted/idle path. INFO **users** count = enabled ACL entries (not a fake number).
- `poll()` / card path non-blocking. Do not starve `mesh.loop()`.
- One cascade file. Stop when the deliverables below exist.
- On Windows: `py -3.13 -m platformio run -e …`. Do **not** run two `heltec_v4_access` builds in parallel.

## Semantics (must match)

### Offline ACL

| Item | Rule |
|---|---|
| Lookup | Wiegand UID hex (canonical uppercase, no spaces) |
| Miss / disabled | `DENY` → red lamp, `showDenied`, **no** OPEN pulse, event `CARD` / `Denied` |
| Hit enabled | `ALLOW` → green + OPEN pulse (existing HAL), `showGranted` (name if stored, else UID), event `Authorized` |
| Empty ACL | every unknown UID DENY (including the previous stub “any card”) |
| Persist | SPIFFS, survive reboot. Native: RAM + existing fake-FS hook if useful |
| Cap | modest (e.g. 64 or 128 records). Drop/refuse overflow honestly — no silent wrap of UIDs |
| Record | `uid`, `enabled`, optional `employee_id` / `name` (short). **No photo bytes** |

E-STOP / EMERGENCY still wins over ALLOW (existing turnstile SM).

### MESH 485 ops (3MAP subset this run)

Implement as `op` strings (envelope already parsed). Privilege = existing `mesh485_role`:

| op | Role | Behaviour |
|---|---|---|
| `ACCESS_STATUS` | VIEWER | `{device: TS-001, state, users, last_uid?}` — no secrets |
| `ACCESS_OPEN` (alias `ACCESS_TURNSTILE_OPEN`) | OPERATOR+ | pulse OPEN via HAL; OLED `showRemote`; enqueue `REMOTE_OPEN` / CARD-like event `source=USB` or `BLE` / `LORA` if known; dual ACK |
| `ACCESS_LOCK` | OPERATOR+ | lock / abort opening if HAL supports it; else honest `NO_PERMISSION` / `HARDWARE` — do not fake |
| `ACCESS_GET_EVENTS` | VIEWER | same ring as `EVENT_LIST` filtered to `TS-001` (or `since`). Do **not** invent a second store |
| `ACCESS_ADD_CARD` | ADMIN | `{uid, employee_id?, name?, enabled?}` persist. Needed to fill ACL without prompt 18 UI |
| `ACCESS_REMOVE_CARD` | ADMIN | `{uid}` |

VIEWER `ACCESS_OPEN` / `ACCESS_LOCK` / card mutate → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`).

Unknown `ACCESS_*` (OTA, REBOOT, LAMP_*, GET_USERS list dump if not implemented, …) stay `UNKNOWN_CMD` or `NO_PERMISSION` — do not implement OTA/reboot/photos.

`COMMAND_EXECUTE` is **not** OPEN. `COMMAND_EXECUTE` `OPEN` / `ACCESS_OPEN` as capability → still `NO_PERMISSION`. Bolid `CONTROL_RELAY` unchanged.

### Adapter `TS-001`

- Still `protocol: ACCESS`. May advertise `READ_STATUS` and, after this run, a capability that **clients** use to show OPEN (e.g. `ACCESS_OPEN`) — **executable only via `ACCESS_OPEN` op**, not `COMMAND_EXECUTE`.
- Points `TURNSTILE` + `READER` stay. State follows HAL (`LOCKED` / `OPEN` / `EMERGENCY`).
- `noteCard` / queue: real UUID, `source=RFID`, `device_ok: true` on a local decision. Unknown UID DENY still enqueues (audit), does not GRANT.

### Bridge / Bolid

`ACCESS_*` must not `writeFrame` on UART1. Bolid adapters / CONFIG protocol / AUTO refusal unchanged.

## Deliverables

### 1. Spec

- Update `docs/protocol-3map.md` (which cmds are live vs later).
- Update `docs/mesh485-protocol.md` (`ACCESS_*` table; OPEN ≠ `COMMAND_EXECUTE`).
- Update `docs/device-adapters.md` (TS-001 ACL, no GRANT-all).
- Update `docs/oled-ui.md` (REMOTE from `ACCESS_OPEN`; users = ACL count).
- Update `docs/security.md` (offline DENY-by-default; remote OPEN role).
- Golden examples: `access_open.request.json` / `.response.json` (and denied VIEWER). `device_info` TS-001 still valid.

### 2. Firmware

- Replace stub in `access_controller.*` with persistent ACL.
- Dispatcher: handle the ops above; stop blanket-refusing all `ACCESS_*`.
- Wire `ACCESS_OPEN` → turnstile pulse + `showRemote` (device build). Native: fake HAL / flags, no GPIO.
- Card path: DENY default; events on `EventStore`.

### 3. Tests (`native_protocol_tests`)

Keep 06–16. Change the old `ACCESS_OPEN` → `NO_PERMISSION` hook to:

- VIEWER `ACCESS_OPEN` → `NO_PERMISSION`
- OPERATOR/ADMIN `ACCESS_OPEN` → OK (native fake) + dual ACK; event queued
- Unknown UID DENY (no pulse); allowlisted UID ALLOW
- Empty ACL DENY (not GRANT-all)
- `ACCESS_ADD_CARD` ADMIN persist (fake FS reboot if cheap); VIEWER refused
- `ACCESS_GET_EVENTS` / `EVENT_LIST` see the card event; UUID present
- `COMMAND_EXECUTE` still not OPEN; Bolid `CONTROL_RELAY` / AUTO / five DISCOVER ids unchanged
- `ACCESS_OTA` / fire-disable names still refused

`py -3.13 -m platformio run -e native_protocol_tests` then `program.exe` → `native_protocol_tests: ok`.

### 4. On-device

- `pio run -e heltec_v4_access` **build**. Flash **COM23** when free.
- USB mux: unknown card → DENY / red (not silent GRANT). After `ACCESS_ADD_CARD`, that UID ALLOW.
- `ACCESS_OPEN` as admin pulses OPEN (bench: watch GPIO 40 / OLED REMOTE). VIEWER refused.
- Isolated A/B / Bolid empty-bus behaviour unchanged (`S20-001` still offline on empty bus).

### 5. App / cascade

- Flutter Access screen: OPERATOR+ **OPEN** may call live `ACCESS_OPEN` (confirm like Control). VIEWER button stays disabled. No employees/photos module.
- Widget test: OPEN disabled for viewer; operator path exists (fixture may ACK timeout or OK — be honest).
- `docs/implementation.md` Phase 17. `AGENTS.md` / `CASCADE.md` / Cursor `Next agent file` → `18-access-app.md`.

## Forbidden this run

- GRANT-all leftover; LittleFS as a second FS.
- Photos on SPIFFS / LoRa / OLED / MESH 485 JSON.
- Flutter employees CRUD, PiDoors PHP, Excel.
- `COMMAND_EXECUTE` as the OPEN path; fire-disable; Bolid native CONTROL; AUTO discovery; NVS protocol persist.
- Changing Access or RS-485 GPIO map; forking MeshCore; MeshCore chat UI.
- OTA / REBOOT / LAMP_* as product ops (3MAP catalog only).
- Treating mesh presence as authorization for OPEN.

## Done when

1. Unknown UID DENY; allowlisted UID ALLOW; ACL persists on SPIFFS.
2. `ACCESS_STATUS` / `ACCESS_OPEN` / `ACCESS_LOCK` / `ACCESS_GET_EVENTS` (+ ADMIN add/remove card) work with roles + dual ACK.
3. Remote OPEN shows OLED `showRemote`; local GRANT/DENY screens still work.
4. Native tests include 06–16 plus new ACL/ACCESS cases; `heltec_v4_access` builds (flash COM23 if free).
5. Phase 17 note in `docs/implementation.md`. Five adapters still in DISCOVER.

Then **stop**. Next cascade file: `agent/prompts/18-access-app.md`.
