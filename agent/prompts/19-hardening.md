# PROMPT 19 — Hardening (last cascade run)

You are finishing **MESH 485 Extender** in this repo. This is **run 19 of 19**. Do **only** this run. Stop when `SECURITY.md` is a real audit (not a stub), `docs/security.md` matches it, `docs/radio-admin.md` is filled, GPIO **19/20** stay USB, photos stay off the radio, fire/security independence and dual ACK are written as **facts of this image** (not new features) — **not** AUTO discovery, **not** a second RS-485 port, **not** Postgres/PiDoors CRUD, **not** Excel, **not** OTA, **not** MeshCore chat, **not** native Orion CONTROL, **not** fire-disable, **not** GRANT-all.

Phases **01–18 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + bridge READ_ONLY, DeviceRegistry `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`, Orion CRC + dual ACK + Flutter console + RU UI + capability-gated `CONTROL_RELAY` on PP-001 only, Signal-10/20 MONITOR, RAM `CONFIG_SET` protocol, no AUTO, offline ACL + `ACCESS_*`, **Flutter Access: employees / cards / disk photos / OPEN+LOCK / events**).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + **PROFILE_ACCESS** + DeviceAdapter registry.

```
Operator console  (no chat)     MeshCore tools  (identity / repeater CLI)
        │                              │
   MESH 485 JSON                    Companion frames
        │
Heltec  Access OLED + ACL + EventStore + isolated UART1
        │
   fire/security panel must keep working if mesh dies
```

This run is **documentation + audit + tiny verify**. No new ops, no new adapters, no new Flutter screens.

## Frozen decisions (do not re-litigate)

- GPIO **19/20 = USB PHY**. Access map stays `docs/pinout.md`. Do not move RS-485 off 39/38/26.
- Offline ACL `/m485_acl.bin`, cap 64, empty / unknown UID = **DENY**. No GRANT-all.
- Photos **never** on the node, OLED, LoRa, or MESH 485 JSON. Path / `employee_id` only. Bytes stay on phone/PC disk and/or FastAPI `PHOTO_DIR` (server still stub).
- OPEN is `op: ACCESS_OPEN`. LOCK is `ACCESS_LOCK`. **Not** `COMMAND_EXECUTE`.
- Remote OPEN/LOCK / CONTROL need **OPERATOR or ADMIN**. VIEWER → `NO_PERMISSION`. Mesh presence is not authorization.
- Dual ACK already exists. Empty bus / no GPIO → honest `TIMEOUT` / `device_ok: false`.
- Keep five DISCOVER devices. Do not TX Bolid frames from this run.
- EM-Marine UID is **not** crypto. Cloneable cards in-scope for convenience only — write that in `SECURITY.md`, do not “fix” it with crypto this run.
- Default RS-485 privilege is **READ_ONLY**. `RAW_BRIDGE` is lab-only and must stay explicit.
- FastAPI `/employees` stays `{items:[]}`. Do not stand up Docker/Postgres.
- Carry-over from 18 (optional, if COM23 is free): flash the already-built `heltec_v4_access` and mux `python scripts/m485_devices.py COM23 acl` so `ACCESS_GET_USERS` after `ACCESS_ADD_CARD` returns the uid. Do **not** block the run on a missing port.

## Read first

- `AGENTS.md` (items 2, 7, 9, 11: USB pins, fire isolation, one console no chat, photos off radio)
- `SECURITY.md` (stub) + `docs/security.md` (draft notes to promote)
- `docs/radio-admin.md` (placeholder)
- `docs/pinout.md` (GPIO 19/20 frozen USB)
- `docs/oled-ui.md` (no PIN, no photos, no MeshCore chat)
- `docs/apps.md` (Mesh screen = extender health; Access = disk photos)
- `docs/mesh485-protocol.md` (roles, replay 30 s / 120 s ts, `ACCESS_*`, `COMMAND_EXECUTE`)
- `docs/bridge.md` (`READ_ONLY` default; `RAW_BRIDGE` lab)
- `docs/bolid.md` + `docs/device-adapters.md` (panel independence; CXEM = vectors)
- `docs/protocol-3map.md` (OPEN ≠ `COMMAND_EXECUTE`)
- Firmware: `include/access/pins.h`, dispatcher `ACCESS_*` / `COMMAND_EXECUTE`, `AccessController`
- Flutter: `app/mesh485/lib/screens/access_screen.dart` (confirm OPEN/LOCK; photo path local-only)

## Rules

- **No new features.** Do not add ops, adapters, GPIO, filesystems, OTA, AUTO, schedules, or employee FastAPI CRUD.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`.
- Access OLED stays home. GRANT / DENY / REMOTE / EMERGENCY / 10 s blank / USER menu unchanged. Photos never on 64 px. BLE PIN (`123456`) stays in the **Android system dialog**, never on SSD1306.
- CXEM / Orion dumps stay **test vectors**. Do not paste decrypted vendor firmware. Do not ship exploit write-ups.
- One cascade file. Stop when the deliverables below exist.
- On Windows: `py -3.13 -m platformio run -e …`. Do **not** run two `heltec_v4_access` builds in parallel.
- If you touch firmware only for a comment / assert / compile warning: `native_protocol_tests` must still print `ok`. Prefer **docs-only**.

## Semantics (must match — write them down, do not change them)

### Authz vs mesh presence

| Action | Role | Refusal |
|---|---|---|
| VIEW / STATUS / EVENT_LIST / ACCESS_STATUS | VIEWER | — |
| `ACCESS_OPEN` / `ACCESS_LOCK` / `COMMAND_EXECUTE` | OPERATOR+ | VIEWER → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`) |
| `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD` / `ACCESS_GET_USERS` | ADMIN | VIEWER/OPERATOR → `NO_PERMISSION` |
| Fire-disable / disarm-all names | — | `NO_PERMISSION` or `UNKNOWN_CMD` even if invented |
| Being on LoRa / BLE / USB | — | **not** authorization |

Replay already: duplicate `(id, operator)` within 30 s, or `ts` outside 120 s when unix time is known.

### Fire / security isolation

- С2000 / Signal-10 / Signal-20 **keep running** if Heltec, LoRa, or the app die.
- This node is an integration gateway, **not** a certified fire appliance. Say so.
- Default `BRIDGE_OPEN` = `READ_ONLY` (no TX). `CONTROL` session refused unless compile flag. `COMMAND_EXECUTE` is a **separate** capability path (PP-001 named `R1`… only).
- `ACCESS_*` must not TX UART1. Bolid adapters must not become OPEN.

### Photos / PIN / USB pins

- Audit Access + EventStore + MESH 485 goldens: **no** jpeg/base64/`photo` field on the wire. Flutter `PhotoStore` is local path only.
- OLED: no BLE PIN, no employee photo, no MeshCore QR/chat.
- GPIO 19/20 remain USB D−/D+. Confirm `pins.h` + `docs/pinout.md` still match. Do not “repurpose” them.

### Radio admin vs operator Mesh

`docs/radio-admin.md` must split:

| Job | Tool |
|---|---|
| Repeater identity, keys, MeshCore CLI | MeshCore companion / official tools (USB or BLE). Not the Flutter Mesh tab. |
| Extender health (RSSI, SNR, RX/TX, uptime, last_seen) | Flutter **Меш** (`NODE_STATUS` / `DIAGNOSTICS`) |
| Operator objects / Access / Control | Flutter console — **no** contacts, channels, chat |

## Deliverables

### 1. `SECURITY.md` (repo root) + `docs/security.md`

Replace the stub. One canonical audit (root may point at `docs/security.md` or hold the full text — do not leave two contradictory drafts). Cover at least:

1. Cloneable EM-Marine / Wiegand UID — convenience ACL, not crypto.
2. BLE NUS + USB CDC pairing (PIN 123456 system dialog; mux `{` vs Companion).
3. MESH 485 roles, replay window, `BAD_AUTH` still stubbed this cascade.
4. GPIO 19/20 = USB PHY; Access/RS-485 maps frozen.
5. Photos never on LoRa / OLED / SPIFFS / MESH 485 JSON.
6. OLED does not leak PIN or secrets.
7. Fire panel independence; no remote disable-alarm / disarm-all.
8. Dual ACK: `mesh_ok` vs `device_ok` vs `TIMEOUT`.
9. `RAW_BRIDGE` lab-only; default READ_ONLY.
10. CXEM dumps = test vectors, not shipped exploits / not firmware source.

Do **not** claim certified fire listing. Do **not** invent OTA/auth that is not in the image.

### 2. `docs/radio-admin.md`

Fill the placeholder. MeshCore tools vs extender Mesh screen. Link `docs/apps.md`.

### 3. Tiny verify (no feature work)

- Grep / read: no photo bytes in `protocol/mesh485/examples/`, EventStore JSON, `ACCESS_GET_USERS` payload.
- Confirm `pins.h` USB 19/20 unused by Access/RS-485.
- `C:\flutter\bin\flutter.bat test` from `app/mesh485` still green (15 tests). Do not add screens.
- If you edit C++: `py -3.13 -m platformio run -e native_protocol_tests` then `program.exe` → `native_protocol_tests: ok`. Skip `heltec_v4_access` rebuild unless you changed firmware.
- Optional: flash **COM23** when free; mux `acl` for GET_USERS after ADD_CARD.

### 4. Spec / cascade close

- `docs/implementation.md` Phase 19. `docs/progress-ru.md` short note.
- `AGENTS.md` item 12 + `CASCADE.md` + Cursor `Next agent file`: cascade **complete** (no prompt 20). Post-cascade leftovers: AUTO, dual UART, Postgres photos, OTA, native Orion CONTROL.
- Do not start those leftovers.

## Forbidden this run

- New MESH 485 ops, adapters, GPIO, filesystems, NVS protocol persist.
- Photos on SPIFFS / LoRa / OLED / JSON.
- PiDoors PHP, MariaDB, Excel, OTA, MeshCore chat/QR.
- `COMMAND_EXECUTE` as OPEN/LOCK; fire-disable; Bolid native CONTROL; AUTO discovery.
- Changing Access or RS-485 GPIO map; forking MeshCore.
- Schedules / access groups / holiday calendars.
- Treating mesh presence as authorization.
- Shipping CXEM/Orion dumps as attack recipes.

## Done when

1. `SECURITY.md` is an audit a reviewer can read without the cascade chat.
2. `docs/radio-admin.md` tells a tech which tool is MeshCore vs which is the extender Mesh screen.
3. GPIO 19/20 and “photos not on radio” are explicit and still true in code.
4. Fire independence + dual ACK + READ_ONLY / RAW_BRIDGE lab-only are written as product facts.
5. Phase 19 note in `docs/implementation.md`. Five adapters still in DISCOVER. Cascade table: 19 **Done.** No next prompt file.

Then **stop**. This was the last cascade file. Do not invent `prompts/20-*.md`.
