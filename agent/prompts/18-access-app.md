# PROMPT 18 — Access module in the same Flutter app

You are implementing **MESH 485 Extender** in this repo. This is **run 18 of 19**. Do **only** this run. Stop when the Flutter Access module can list/add/disable employees and Wiegand cards against the live node ACL, show photos from **disk/server only**, drive **OPEN and LOCK** via `ACCESS_*` (not `COMMAND_EXECUTE`), and tail live Access events — **not** `SECURITY.md` (19), **not** PiDoors PHP, **not** Excel, **not** OTA, **not** AUTO discovery, **not** Bolid CONTROL, **not** fire-disable, **not** MeshCore chat, **not** photos on LoRa / OLED / MESH 485 JSON.

Phases **01–17 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + bridge READ_ONLY, DeviceRegistry `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`, Orion CRC + dual ACK + Flutter console + RU UI + capability-gated `CONTROL_RELAY` on PP-001 only, Signal-10/20 MONITOR, RAM `CONFIG_SET` protocol, no AUTO, **offline ACL + ACCESS_STATUS / OPEN / LOCK / GET_EVENTS / ADD_CARD / REMOVE_CARD**).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + **PROFILE_ACCESS** (Wiegand + ACL + 3MAP) + DeviceAdapter registry.

```
Flutter Access (this run: employees / cards / photos-on-disk / OPEN+LOCK / events)
        │  same app as Dashboard / Devices — Android = Windows
MESH 485  ACCESS_*  (op strings; 3MAP is a profile namespace, not a second mesh)
        │
Heltec  AccessController SPIFFS /m485_acl.bin  +  EventStore  +  TurnstileHAL
```

Look: **PiDoors-inspired** (employees, cards, access log) with product labels **турникет** / TS-001 — not a PHP port. Schema ideas: `vendor/pidoors` + `server/app/models/entities.py`. Do **not** vendor PiDoors React/PHP/MariaDB.

## Frozen decisions (do not re-litigate)

- GPIO **19/20 = USB PHY**. Access map stays `docs/pinout.md`. Do not move RS-485 off 39/38/26.
- **SPIFFS** ACL `/m485_acl.bin`. Cap 64. Empty / unknown UID = **DENY**. No GRANT-all.
- Photos **never** on the node, OLED, LoRa, or MESH 485 JSON. Photo bytes stay on phone/PC disk and/or FastAPI `PHOTO_DIR`. Wire only a path / employee_id.
- OPEN is `op: ACCESS_OPEN` (alias `ACCESS_TURNSTILE_OPEN`). LOCK is `ACCESS_LOCK`. **Not** `COMMAND_EXECUTE`.
- Remote OPEN/LOCK still need **OPERATOR or ADMIN**. VIEWER → `NO_PERMISSION`. Mesh presence is not authorization.
- Dual ACK already exists. Fixture without GPIO: honest `TIMEOUT` / `device_ok: false`.
- Keep five DISCOVER devices. Do not TX Bolid frames from this run. Do not drop RAW / PP / S10 / S20.
- FastAPI (`server/`) is still a stub (`/employees` → `{items:[]}`). If you do not stand up PostgreSQL CRUD, keep **node-only** employees (ACL on the Heltec + local photo files) and write a short follow-up in `docs/apps.md` / Phase 18. Do not block the run on Docker.
- EM-Marine UID is **not** crypto. Cloneable cards in-scope for convenience only (full `SECURITY.md` is 19).

## Read first

- `AGENTS.md` item 9 (one console, no chat) + `docs/apps.md` + `docs/architecture.md` (Operator app)
- `docs/protocol-3map.md` (GET_USERS still “later” after 17 — this run may add a modest ADMIN list dump)
- `docs/mesh485-protocol.md` (`ACCESS_*` table; OPEN ≠ `COMMAND_EXECUTE`)
- `docs/device-adapters.md` + `docs/oled-ui.md` (REMOTE already from `ACCESS_OPEN`)
- `docs/security.md` (offline DENY-by-default; photos not on radio)
- `app/mesh485/lib/screens/access_screen.dart` (OPEN confirm only; no employees)
- `app/mesh485/lib/client/*.dart` (`accessOpen` already live)
- Firmware: `access_controller.*`, dispatcher `ACCESS_*`, `EventStore`
- `vendor/pidoors` — **UI/schema ideas only** (employees, cards, logs). Labels = турникет.
- `server/app/main.py` + `server/app/models/entities.py` (Employee / Card / AccessEvent; photo_path)

## Rules

- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`.
- Access OLED stays home. GRANT / DENY / REMOTE / EMERGENCY / 10 s blank / USER menu unchanged. Photos never on 64 px.
- One cascade file. Stop when the deliverables below exist.
- On Windows: `py -3.13 -m platformio run -e …`. Do **not** run two `heltec_v4_access` builds in parallel.
- Flutter: `C:\flutter\bin\flutter.bat test` from `app/mesh485`. Verify Access UI in the running app (Windows and/or web) — not a screenshot-only pass.

## Semantics (must match)

### Employees + cards

| Item | Rule |
|---|---|
| Identity | `employee_id` (short) + display `name`. Optional department/position if cheap. |
| Card | Wiegand UID hex, canonical uppercase, no spaces — same as firmware ACL |
| Bind | Add/update card → `ACCESS_ADD_CARD` `{uid, employee_id?, name?, enabled?}`. Remove → `ACCESS_REMOVE_CARD` `{uid}` |
| List | ADMIN must see what is on the **node**. Prefer new `ACCESS_GET_USERS` (VIEWER may see count-only via `ACCESS_STATUS.users`; full dump ADMIN). Payload modest: array of `{uid, enabled, employee_id?, name?}` — **no photo bytes**, cap 64. If you skip GET_USERS, persist a local mirror and document that a fresh app install cannot list ACL until cards are re-added — that is worse; implement GET_USERS. |
| Unknown UID | still DENY on the node (do not fake GRANT in the UI) |
| Overflow | node `BUSY` — show it; do not silent-drop |

### Photos

- Pick/crop optional; store file under an app documents dir or `PHOTO_DIR`.
- UI may show the image next to the employee **in Flutter only**.
- Never put base64/jpeg into MESH 485, EventStore, SPIFFS, OLED, or LoRa.

### OPEN / LOCK / events

- OPEN: already wired (confirm like Control). Keep it. VIEWER disabled.
- LOCK: OPERATOR+ confirm → `ACCESS_LOCK`. If HAL cannot lock, firmware already returns honest `NO_PERMISSION` / `HARDWARE` — show that. Do not fake.
- Events: Access tab (or Events filter) from `ACCESS_GET_EVENTS` / `EVENT_LIST` `device_id=TS-001`. Show UUID, uid, Authorized/Denied, `source`, `mesh_ok` / `device_ok`. Empty `[]` is OK.
- Poll or USB `EVENT_SUBSCRIBE` is fine; do not starve the node.

### Roles

| Action | Role |
|---|---|
| View TS-001 / events / STATUS | VIEWER |
| OPEN / LOCK | OPERATOR+ |
| Add/remove/edit card, GET_USERS dump, enable/disable | ADMIN |
| VIEWER mutate → disabled controls + existing `NO_PERMISSION` |

### Firmware (allowed this run)

- `ACCESS_GET_USERS` (ADMIN list dump, no photos) if the app needs it.
- Tiny compile fixes. No pinout change. No second filesystem. No OTA/REBOOT/LAMP_*.
- `COMMAND_EXECUTE` still not OPEN. Bolid adapters unchanged. No UART1 TX from `ACCESS_*`.

### FastAPI

Optional. Health + empty lists already exist. Real CRUD/Postgres/photos-dir is **nice** if cheap; otherwise node-only + disk photos + follow-up note. Do not port PiDoors PHP.

## Deliverables

### 1. App

- Access module: employees list, add/edit/disable, bind UID, photo from disk, OPEN + LOCK, live TS-001 events.
- Same widgets Android + Windows (+ web if already in the project). RU l10n (`flutter gen-l10n`). Protocol ids stay Latin (`TS-001`, `ACCESS_OPEN`).
- Fixture: honest TIMEOUT on OPEN/LOCK without GPIO. ADMIN add-card may succeed against fixture RAM if you extend the fixture — do not claim a physical pulse.
- Widget tests: VIEWER cannot OPEN/LOCK/mutate; OPERATOR OPEN+LOCK path exists; ADMIN can add a card in fixture or documented live.

### 2. Firmware (only if GET_USERS / small glue)

- Native: keep 06–17. Add GET_USERS ADMIN vs VIEWER refused; still no GRANT-all; five DISCOVER ids; AUTO refused; `ACCESS_OTA` refused.
- `py -3.13 -m platformio run -e native_protocol_tests` then `program.exe` → `native_protocol_tests: ok`.
- Device: `pio run -e heltec_v4_access` **build**. Flash **COM23** when free. Mux: `ACCESS_GET_USERS` after `ACCESS_ADD_CARD` returns the uid; Flutter against COM23 if the port is free.

### 3. Spec / cascade

- Update `docs/apps.md` (Access = employees/cards/photos-disk/OPEN+LOCK/events).
- Update `docs/protocol-3map.md` if `ACCESS_GET_USERS` goes live.
- Update `docs/mesh485-protocol.md` / goldens if a new op.
- `docs/implementation.md` Phase 18. `AGENTS.md` / `CASCADE.md` / Cursor `Next agent file` → `19-hardening.md`.

## Forbidden this run

- Photos on SPIFFS / LoRa / OLED / MESH 485 JSON.
- PiDoors PHP, MariaDB, Excel, OTA, MeshCore chat/QR.
- `COMMAND_EXECUTE` as OPEN/LOCK; fire-disable; Bolid native CONTROL; AUTO discovery; NVS protocol persist.
- Changing Access or RS-485 GPIO map; forking MeshCore.
- Schedules / access groups / holiday calendars (PiDoors extras) — out of scope.
- `SECURITY.md` rewrite (19). Treating mesh presence as authorization.

## Done when

1. ADMIN can add a named employee + UID; node ACL allows that card; unknown UID still DENY.
2. Photo visible in the app from disk; never on the wire to the node.
3. OPERATOR OPEN and LOCK call `ACCESS_OPEN` / `ACCESS_LOCK` with confirm + dual ACK (live or honest fixture TIMEOUT).
4. Access events list shows real UUIDs from the node (or fixture rows with the same shape).
5. Phase 18 note in `docs/implementation.md`. Five adapters still in DISCOVER. Next: `agent/prompts/19-hardening.md`.

Then **stop**. Next cascade file: `agent/prompts/19-hardening.md`.
