# Security audit — MESH 485 Extender

Audit of **this firmware/console image** (phase 23). Not a threat-model wishlist. Not a certified listing.

Product: Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + isolated RS-485 HAL + `PROFILE_ACCESS`. Node id `EXT-001`. Six DISCOVER devices: `RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`, `PCN-001` (GPIO 47 read-only; no CONTROL).

Canonical copy in-repo: this file. `docs/security.md` points here so the two cannot drift.

## Scope and non-claims

This node is an **integration gateway**. It is **not** a certified fire appliance, not a listed fire panel, and not a high-security ACS. Do not market it as a replacement for С2000 / Signal-10 / Signal-20 panel logic.

**Not in this image** (do not assume they exist): OTA, signed firmware update, MeshCore packet-signature verify of MESH 485 JSON, NVS protocol persist, AUTO discovery, photos on SPIFFS/LoRa, Postgres/FastAPI employee CRUD, native Orion CONTROL, a second RS-485 port, GRANT-all.

## 1. Cloneable EM-Marine / Wiegand UID

The reader is external 125 kHz Wiegand 26/34. The Heltec does not demodulate 125 kHz. The ACL stores a **canonical uppercase hex UID** (no spaces) in RAM + SPIFFS `/m485_acl.bin` (cap 64). Lookup is equality on that hex.

**EM-Marine UID is not cryptographic authentication.** Cards are cloneable. This is convenience access control. Cloneable cards are in-scope for convenience only; this cascade does not add crypto, rolling codes, or challenge-response.

Default store is empty. Empty store / unknown UID / disabled row = **DENY** (red lamp, `showDenied`, no GPIO 40 pulse). There is no GRANT-all. Lab seed only: `AccessController::allowUid` or `-D MESH485_ALLOW_TEST_UID=1` (`04A79122`). Overflow on add → `BUSY` (no silent wrap).

`ACCESS_STATUS` exposes `users` (enabled count) and optional `last_uid`. That last UID is convenience telemetry, not a secret worthy of OLED/PIN treatment — but it is still a physical-token identifier.

## 2. BLE NUS + USB CDC pairing

One binary: MeshCore companion on **BLE and USB**.

| Port | Name / use | Pairing |
|---|---|---|
| BLE NUS | `3MESH-TS-001` | PIN **123456** in the **Android system** pairing dialog (`-D BLE_PIN_CODE=123456`) |
| USB CDC | Windows primary (bench COM23) | Physical USB; opening CDC often resets the S3 |

PIN is a MeshCore companion default, not a MESH 485 credential. It must **not** appear on the SSD1306 (it does not). Change the compile pin in a later hardening pass if the site is not a lab; this image ships `123456`.

**Mux:** a burst that starts with `{` is one NDJSON MESH 485 line (USB and BLE). Everything else is Companion binary. Do not wrap MESH 485 in Companion `0x3c`/`0x3e`. Hosts must skip non-`{` boot lines after CDC reopen.

Being connected on BLE, USB, or LoRa is **not** authorization (see §3).

## 3. MESH 485 roles, replay, `BAD_AUTH`

Envelope `operator` is a **name**, not the source of truth for the role. The node keeps an allow-list (cap 8) of `{name, role, pubkey?}` in RAM and on SPIFFS as `/m485_auth.bin` (magic `AUT1`; write version **2** with 32-byte pubkey, zeros = unbound; version **1** phase-21 blobs still load as unbound rows). Same packed-header style as `/m485_acl.bin`, not ESP-IDF `nvs_*`. The file is **not encrypted**. Factory seed (missing / bad magic / cap overflow / no ADMIN): `viewer` / `operator` / `admin`, no pubkey. `AUTH_SET` `{name, role}` upsert; `{name, enabled:false}` drops a row (not the last ADMIN); `{name, pubkey:"<64 hex>"}` binds; empty/`null` pubkey unbinds (role unchanged). Persist refuses a table with no ADMIN. No password. No MeshCore packet signature on USB/BLE.

Resolve (after replay 30/120): unknown name, claimed-up keyword, **or a bound row without a matching key** → envelope `BAD_AUTH`. Presented key is **`payload.pubkey`** (64 hex) or, if that is omitted, `Mesh485Env.sender_pub` when the transport fills it. Equality check only — a pubkey in JSON on USB/BLE is **not** a MeshCore signature. Unbound rows (factory lab) still accept the name alone. Factory `viewer` OPEN/LOCK/ADD is still `NO_PERMISSION`. `"operator":"admin"` without a matching allow-list row is `BAD_AUTH`.

Replay (unchanged): reject duplicate `(id, operator)` inside **30 s**, or `ts` outside **±120 s** when unix time is known (`unix_now == 0` skips the wall-clock window). Replay → envelope `REPLAY` (checked before AuthZ).

| Action | Role | Refusal |
|---|---|---|
| VIEW / STATUS / EVENT_LIST / ACCESS_STATUS / ACCESS_GET_EVENTS | VIEWER | unknown name → `BAD_AUTH` |
| `ACCESS_OPEN` / `ACCESS_LOCK` / `COMMAND_EXECUTE` | OPERATOR+ | VIEWER → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`); unknown → `BAD_AUTH` |
| `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD` / `ACCESS_GET_USERS` | ADMIN | VIEWER/OPERATOR → `NO_PERMISSION`; unknown → `BAD_AUTH` |
| `AUTH_SET` | ADMIN | VIEWER/OPERATOR → `NO_PERMISSION` |
| Fire-disable / disarm-all names | — | `NO_PERMISSION` or `UNKNOWN_CMD` even if invented |
| `ACCESS_OTA` / `ACCESS_REBOOT` / `ACCESS_LAMP_*` | — | `UNKNOWN_CMD` |
| Being on LoRa / BLE / USB | — | **not** authorization |

OPEN is `op: ACCESS_OPEN` (alias `ACCESS_TURNSTILE_OPEN`). LOCK is `ACCESS_LOCK`. **Not** `COMMAND_EXECUTE`. `COMMAND_EXECUTE` with capability `ACCESS_OPEN` / `OPEN` is still `NO_PERMISSION`.

## 4. GPIO 19/20 = USB PHY

Frozen Access + RS-485 map (`include/access/pins.h`, `docs/pinout.md`):

| Function | GPIO |
|---|---|
| USB D− / D+ | **19 / 20** — never Access, never RS-485 |
| Wiegand D0/D1 | 15 / 16 |
| OPEN / green / red | 40 / 41 / 33 |
| ENTRY / EXIT / E-STOP | 3 / 4 / 6 |
| OLED SDA/SCL/RST | 17 / 18 / 21 |
| RS-485 TX/RX/DE | **39 / 38 / 26** |

Firmware defines `PIN_USB_DM 19` and `PIN_USB_DP 20` so they are not reassigned. A second UART is an expander later — not these pins.

## 5. Photos never on the radio

Photos are **not** on the node, OLED, SPIFFS, LoRa, or MESH 485 JSON.

| Surface | What travels |
|---|---|
| `ACCESS_GET_USERS` | `{uid, enabled, employee_id?, name?}` only — page 16, cap 64 |
| `ACCESS_ADD_CARD` | `uid`, optional `employee_id` / `name` / `enabled` |
| EventStore JSON / `EVENT_LIST` / `ACCESS_GET_EVENTS` | uuid, uid, name, source, dual ACK — no `photo` / jpeg / image field |
| Flutter `PhotoStore` | local filesystem path keyed by `employee_id` or uid |
| FastAPI `/employees` | still `{items:[]}`; `PHOTO_DIR` is a server stub, not wired |

Goldens: `protocol/mesh485/examples/access_get_users.response.json`, `access_add_card.request.json`. Native test asserts GET_USERS has no `base64` / `.jpg`. Flutter never puts the path into `rpc()`.

## 6. OLED does not leak PIN or secrets

SSD1306 128×64 shows Access product UI only (`docs/oled-ui.md`): READY / GRANT / DENY / REMOTE / LOCKED / EMERGENCY / USER menu / INFO / bridge level. After 10 s idle → `DISPLAYOFF` (VEXT stays on).

**Not on the panel:** BLE PIN, admin passwords, MeshCore QR/chat, employee photographs, raw Companion frames.

INFO may show device id, enabled user **count**, last UID, heap, uptime. Last UID is the Wiegand hex, not a mesh key.

## 7. Fire / security panel independence

С2000 / Signal-10 / Signal-20 **keep running** if Heltec, LoRa, USB, BLE, or the Flutter app die. This node sits beside the bus; it is not the panel CPU and not the only path for alarm/guard functions.

There is **no** remote disable-alarm / disarm-all / silence-all MESH 485 op. Forbidden capability names (`DISABLE_FIRE_ALARM`, `DISARM_ALL`, `ARM_ALL`, `SILENCE`, `FIRE_RESET`, …) are hard-refused even if a client invents them (`roles.h`).

`ACCESS_*` does not TX UART1. Bolid adapters do not become turnstile OPEN. Signal-10 (`S10-001`) and Signal-20 (`S20-001`) advertise **MONITOR** only — no `CONTROL_RELAY`. PP-001 may advertise `CONTROL_RELAY` only after an explicit named-relay allow (`R1`…`R8`); default allow-list is **empty**.

CRC-valid Orion/Modbus echo still does not mean the panel executed a function (relay program, section, permissions).

## 8. Dual ACK

Every command path that talks to hardware or the bus reports **two** flags, not one bool:

| Flag | Meaning |
|---|---|
| `mesh_ok` | Request reached this node and was dispatched |
| `device_ok` | HAL / instrument produced an honest success |
| `error: TIMEOUT` | TX happened or was allowed, no usable reply (empty bus, no GPIO) |
| `error: NO_PERMISSION` | Role/capability/point refused — `mesh_ok: true`, `device_ok: false` |

Empty A/B or missing GPIO must stay `TIMEOUT` / `device_ok: false`. Do not fake a device ACK. Events store the same pair (`EventRecord.mesh_ok` / `device_ok`). Last command is RAM-only on `DIAGNOSTICS.last_command` (operator, device, cap, point, both ACKs).

## 9. `RAW_BRIDGE` lab-only; default READ_ONLY

`BRIDGE_OPEN` default (omit `level`) = **`READ_ONLY`**: RX ring only, **no** `writeFrame`, **no** `RS485_TRANSACTION` while the session is open.

| Level | This image |
|---|---|
| `READ_ONLY` | Default. Capture only. |
| `CONTROL` | `NO_PERMISSION` unless `-D MESH485_ALLOW_BRIDGE_CONTROL=1`. Still not `COMMAND_EXECUTE`. |
| `RAW_BRIDGE` | Lab only. Must be **explicit**. Then Phase-06 transactions are allowed. Last session (operator + ts + level) is RAM `DIAGNOSTICS.last_session`, not OLED. |
| unknown | `UNKNOWN_CMD` |

DE (GPIO 26) is HIGH only during TX; idle / close / abort / mesh loss leave DE **RX**. `RAW_BRIDGE` is not the Orion path (turnaround ~50 ms vs mesh RTT).

## 10. CXEM / Orion dumps = test vectors

`docs/reference/orion-vectors/` and published hex (Habr / CXEM) are **unit-test fixtures** for CRC-8/MAXIM and frame layout. They are not shipped exploits and not vendor firmware source. Do not paste decrypted Bolid firmware into this repo. Do not publish attack recipes.

`hass-hubc2000pp` is an Apache-2.0 **model** of zones/relays/sections (IP to HUB-C2000PP). It is not a UART stack on this node.

## Residual risk (honest)

1. **Factory names are public until bound.** Unknown / upgraded names get `BAD_AUTH`, but unbound lab identities `admin` / `operator` / `viewer` have no password. Anyone who can send `{` NDJSON can still act as factory admin until an ADMIN removes those rows or binds a pubkey. A bound row requires matching `payload.pubkey` (or env sender). That JSON field on USB/BLE is **not** a MeshCore signature — it is an equality check. `/m485_auth.bin` is not a secret.
2. **BLE PIN 123456** is a lab default.
3. **Cloneable cards.** Physical token clone = GRANT if the UID is in ACL.
4. **USB mux shares Companion.** A hostile host on CDC can send both Companion frames and MESH 485.
5. **SPIFFS blobs are not encrypted.** `/m485_acl.bin` (UIDs), `/m485_auth.bin` (operator names/roles), and `/m485_cfg.bin` (selected bus protocol — not a secret) are readable if someone has the flash.
6. **No fire listing.** Independent panels must stay the authority.

## Related

- Protocol roles / ops: `docs/mesh485-protocol.md`
- Bridge levels: `docs/bridge.md`
- Pins: `docs/pinout.md`
- OLED: `docs/oled-ui.md`
- Radio vs Mesh screen: `docs/radio-admin.md`
- Bolid independence: `docs/bolid.md`
- Console: `docs/apps.md`
