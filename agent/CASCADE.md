# Cascade — one prompt at a time

Product: **MESH 485 Extender**. Heltec V4 is a universal RS-485 gateway over MeshCore. Bolid and the turnstile are **profiles / adapters**, not separate firmwares.

Context: `AGENTS.md` + `docs/architecture.md` + `docs/pinout.md` + `docs/mesh485-protocol.md`.

Phases **01–23** and **25–28** are done. Cascade **01–19** closed. **Next is not assigned.** Prompt **24** stays open as a separate Orion listen-only job (refreshed after the bench; not the PCN relay). The operator chooses that job or a stop. Leftovers: AUTO, dual UART, Postgres/photos, OTA, native Orion CONTROL, MeshCore chat/QR.

| # | File | Result |
|---|---|---|
| 01 | `FIRST_RUN.md` / `prompts/01-pinout.md` | **Done.** Access pinout frozen; OLED Access UI |
| 02 | `prompts/02-firmware-ble-usb.md` | **Done.** BLE+USB MeshCore companion, Access OLED owns the panel |
| 03 | `prompts/03-oled-ui.md` | **Done.** All OLED states, USER menu, `M●` / `R` |
| 04 | `prompts/04-wiegand-turnstile.md` | **Done.** Wiegand + relay + lamps + sensors (ACL still stub) |
| 05 | `prompts/05-rs485-hal.md` | **Done.** Isolated RS-485 UART + DE/RE; OLED 10 s blank |
| 06 | `prompts/06-mesh485-protocol.md` | **Done.** JSON envelope + dispatcher + `RS485_TRANSACTION` |
| 07 | `prompts/07-remote-bridge.md` | **Done.** Remote RS-485 bridge; default **READ_ONLY** |
| 08 | `prompts/08-device-adapters.md` | **Done.** `DeviceAdapter` API, Device / Point / Event, two RAM stubs |
| 09 | `prompts/09-orion-research.md` | **Done.** Orion frames + CRC-8/MAXIM + timings; Path A stub for 10 |
| 10 | `prompts/10-bolid-readonly.md` | **Done.** Path A `bolid/c2000pp_modbus` (`PP-001`), READ_ONLY probe |
| 11 | `prompts/11-events-queue.md` | **Done.** Local event queue, mesh publish, dual ACK (mesh vs device) |
| 12 | `prompts/12-app-shell.md` | **Done.** Flutter console shell (fixture MESH 485 JSON) |
| 13 | `prompts/13-app-operator.md` | **Done.** Live USB/BLE MESH 485 + RU UI (Flutter + OLED) |
| 14 | `prompts/14-allowed-control.md` | **Done.** Capability model, roles, confirmations; no fire-disable shortcut |
| 15 | `prompts/15-signal10.md` | **Done.** Signal-10 MONITOR (`S10-001`); CONFIG protocol manual, no AUTO |
| 16 | `prompts/16-signal20.md` | **Done.** Signal-20 MONITOR (`S20-001`); CONFIG protocol + `BOLID_SIGNAL20`; no AUTO |
| 17 | `prompts/17-access-acl.md` | **Done.** Offline ACL, UUID events, 3MAP STATUS/OPEN/LOCK/GET_EVENTS + ADMIN add/remove card |
| 18 | `prompts/18-access-app.md` | **Done.** Employees / cards / photos-disk / OPEN+LOCK / events in Flutter |
| 19 | `prompts/19-hardening.md` | **Done.** `SECURITY.md` audit; fire isolation; GPIO 19/20; photos not on radio |
| 20 | `prompts/20-authz.md` | **Done.** RAM operator allow-list; `BAD_AUTH` live; `AUTH_SET` ADMIN-only |
| 21 | `prompts/21-auth-persist.md` | **Done.** Persist allow-list across reboot (`/m485_auth.bin`; no pubkey) |
| 22 | `prompts/22-auth-pubkey.md` | **Done.** Optional pubkey bind on allow-list row (no MeshCore chat) |
| 23 | `prompts/23-config-persist.md` | **Done.** Persist `CONFIG_SET protocol` (`/m485_cfg.bin`; not baud, not AUTO) |
| 24 | `prompts/24-c2000m-monitor.md` | **Open, separate.** Orion RS-485 listen-only → EventStore. Not the PCN contact |
| 25 | `prompts/25-pcn-contact.md` | **Done.** GPIO 47 → one `PCN`/`ALARM` or `NORMAL` in EventStore. Six DISCOVER ids. Not on the mesh yet |
| 26 | `prompts/26-pcn-mesh.md` | **Done.** 59-byte hop `EXT-001` → `EXT-002`, same uuid, ack sets `mesh_ok`. Not chat |
| 27 | `prompts/27-rdm6300.md` | **Done.** RDM6300 frame → existing `TS-001` ACL. No seventh DISCOVER id |
| 28 | `prompts/28-pcn-acs-bench.md` | **Done.** Joint bench by fixture. Live boards not run |

**Do not start with** native Orion CONTROL, AUTO discovery, Excel, OTA, photos-over-LoRa, MeshCore chat UI, or PiDoors PHP.

Superseded files: old `05-acl-events-3map` … `13-hardening` and obsolete 14–18. Use this table.
