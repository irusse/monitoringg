# PROMPT 12 — Flutter MESH 485 shell (Android + Windows)

You are implementing MESH 485 Extender in this repo. This is run 12 of 19. Do only this run. Stop when one Flutter console compiles for Windows (and Android if the SDK/device is present) with the `docs/apps.md` routes — not MeshCore Open chat, not live USB/BLE codec (unless a tiny stub seam), not CONTROL, not Signal-10/20, not ACL 3MAP, not soldering, not a second TFT.

Phases 01–11 are done (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + RS485_TRANSACTION + USB mux, BRIDGE_* default READ_ONLY, DeviceRegistry RAW-001 / TS-001 / PP-001 Path A Modbus READ_ONLY stub, Orion CRC-8 unwired, persistent EventStore + dual ACK mesh_ok vs device_ok, EVENT_SUBSCRIBE on USB).

## Product (do not expand)

Heltec V4 = MeshCore transport + MESH 485 application layer + RS-485 HAL + Access profile + DeviceAdapter registry.

```
Flutter console (this run: chrome + fixture data)
        │  live BLE/USB is prompt 13
MESH 485 JSON
        │
Heltec V4 (already flashing COM23) → EventQueue → USB mux
```

Clients still see Device / Point / Event. This run is **app chrome**. Prompt 13 wires live MESH 485. Prompt 14 is CONTROL. Prompt 17–18 are ACL / employees / OPEN.

## Frozen (do not re-litigate)

- Path A product default. PP-001 empty bus `online:false`; no fake type=36; no IR 3600; no write coils.
- `crc8_orion` stays unwired. `COMMAND_EXECUTE` stays `NO_PERMISSION`.
- GPIO 19/20 = USB. RS-485 stays 39/38/26. Access map unchanged (Wiegand 15/16, OPEN 40, lamps 41/33, ENTRY/EXIT/ESTOP 3/4/6).
- **Do not solder. Do not add GMT147 / ST7789. Do not add MIFARE 13.56 UART/1-Wire.** Bench hardware is later; firmware pinout stays.
- Do not fork `vendor/MeshCore`. No DISPLAY_CLASS / WITH_RS232_BRIDGE / WITH_ESPNOW_BRIDGE.
- No MeshCore chat, contacts, channels, QR, or photos over radio.
- Heltec SSD1306 remains the node UI (`docs/oled-ui.md`). The Flutter Mesh screen is extender health, not radio chat.

## Read first

- `AGENTS.md` (item 9: one console Android = Windows, no chat)
- `docs/apps.md` — screen table
- `docs/architecture.md`
- `docs/mesh485-protocol.md` — envelope; EVENT_LIST / EVENT_SUBSCRIBE; DIAGNOSTICS mesh_ok vs device_ok
- `docs/device-adapters.md` — Device / Point / Event JSON; ids RAW-001 / TS-001 / PP-001
- `docs/oled-ui.md` + `docs/pinout.md` (do not retarget GPIOs from the app)
- `protocol/mesh485/examples/*.json`
- Existing `app/` if any → one package `app/mesh485` (or `app/flutter`)
- `vendor/meshcore-open` — **library only** if you touch Companion; do not vendor the messenger UI

## Semantics (must match)

### App

- One Flutter project, **same widgets** on Windows and Android.
- Navigation: Dashboard, Objects, Devices, Events, Mesh, Bridge, Access, Settings. Login may be a stub role chip (`viewer`).
- **Mesh** = extender-node health placeholders (RSSI/SNR/RX/TX/uptime), not contacts.
- **Bridge** = placeholder (`READ_ONLY` / off). No live BRIDGE_OPEN.
- **Access** = placeholder `TS-001` card. No employees CRUD, no OPEN.
- **Events** = list bound to fixture rows shaped like MESH 485 Event JSON (`id`, `device_id`, `type`, `source`, `mesh_ok`, `device_ok`). Fake data is OK; document the seam.
- **Devices** = three rows `RAW-001` / `TS-001` / `PP-001` (PP may show offline).

### Companion

- Live BLE/USB **stubbed** (`Mesh485Client` + in-memory JSON). Prompt 13 attaches USB CDC (Windows) and BLE (Android).
- Do not ship MeshCore Open home/chat. Do not parse Companion binary frames unless a copy-paste-cheap helper already exists — isolate it.

### Firmware

- No firmware pinout / EventStore / dispatcher changes unless a one-line compile break.
- Do not implement GMT147 gate animation or MIFARE 1W this run.

## Deliverables

1. Flutter app `app/mesh485` (or `app/flutter`) with the routes above.
2. `docs/apps.md` — shell exists; live transport is prompt 13.
3. `docs/implementation.md` Phase 12.
4. `flutter run -d windows` succeeds (log). Android if `flutter devices` shows a target; else document SDK/device missing.
5. `AGENTS.md` / `CASCADE.md` / Cursor Next agent file → `13-app-operator.md`.

## Forbidden this run

- Soldering; GMT147/ST7789; MIFARE UART/1-Wire; motor/relay wiring.
- MeshCore Open UI, chat, contacts, channels, QR.
- Live CONTROL / `COMMAND_EXECUTE` / write coils / fire-disable.
- Signal-10/20; Path B Orion UART; wiring `crc8_orion`.
- ACCESS_* 3MAP / offline ACL / employees (17–18).
- Photos, OTA, Excel, AUTO discovery, NVS protocol persist.
- Changing Access GPIO map; forking `vendor/MeshCore`; RS232/ESP-NOW bridge.
- Byte-streaming RAW over LoRa.

## Done when

1. `flutter run -d windows` launches the shell (or documented blocker).
2. All listed routes exist; Mesh is extender health, not chat.
3. Fixture Device/Event ids match firmware (`RAW-001`, `TS-001`, `PP-001`).
4. Phase 12 note in `docs/implementation.md`.
5. Next cascade file: `agent/prompts/13-app-operator.md`.

Then stop.
