# PROMPT 13 — Live operator console + Russian UI

You are implementing MESH 485 Extender in this repo. This is run 13 of 19. Do only this run. Stop when the Flutter console talks live MESH 485 (USB CDC on Windows; BLE on Android if muxed) with operator screens bound to Device / Point / Event, **and** operator-facing UIs are Russian — not MeshCore Open chat, not CONTROL / `COMMAND_EXECUTE`, not Signal-10/20, not ACL 3MAP / employees / OPEN, not soldering, not a second TFT.

Phases 01–12 are done (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON + RS485_TRANSACTION + USB mux, BRIDGE_* default READ_ONLY, DeviceRegistry RAW-001 / TS-001 / PP-001 Path A Modbus READ_ONLY stub, Orion CRC-8 unwired, persistent EventStore + dual ACK, EVENT_SUBSCRIBE on USB, Flutter shell `app/mesh485` on fixture JSON).

## Product (do not expand)

Heltec V4 = MeshCore transport + MESH 485 application layer + RS-485 HAL + Access profile + DeviceAdapter registry.

```
Flutter console (this run: live USB CDC Windows / BLE Android)
        │  fixture remains fallback if the port is missing
MESH 485 JSON  (NDJSON `{` … `}\n` — not Companion binary chat)
        │
Heltec V4 (USB mux COM23; BLE name 3MESH-TS-001) → EventQueue
```

Clients still see Device / Point / Event. This run is **live operator screens + RU l10n**. Prompt 14 is CONTROL. Prompt 17–18 are ACL / employees / OPEN.

## Frozen (do not re-litigate)

- Path A product default. PP-001 empty bus `online:false`; no fake type=36; no IR 3600; no write coils.
- `crc8_orion` stays unwired. `COMMAND_EXECUTE` stays `NO_PERMISSION`.
- GPIO 19/20 = USB. RS-485 stays 39/38/26. Access map unchanged (Wiegand 15/16, OPEN 40, lamps 41/33, ENTRY/EXIT/ESTOP 3/4/6).
- **Do not solder. Do not add GMT147 / ST7789. Do not add MIFARE 13.56 UART/1-Wire.**
- Do not fork `vendor/MeshCore`. No DISPLAY_CLASS / WITH_RS232_BRIDGE / WITH_ESPNOW_BRIDGE.
- No MeshCore chat, contacts, channels, QR, or photos over radio.
- Heltec SSD1306 remains the node UI (`docs/oled-ui.md`). The Flutter Mesh screen is extender health, not radio chat.

## Read first

- `AGENTS.md` (item 9: one console Android = Windows, no chat)
- `docs/apps.md` — screen table + Phase 12 status
- `docs/architecture.md`
- `docs/mesh485-protocol.md` — envelope; USB mux `{` NDJSON; EVENT_LIST / EVENT_SUBSCRIBE; DIAGNOSTICS `mesh_ok` vs `device_ok`
- `docs/device-adapters.md` — Device / Point / Event; ids RAW-001 / TS-001 / PP-001
- `docs/oled-ui.md` + `docs/pinout.md` (do not retarget GPIOs from the app)
- `protocol/mesh485/examples/*.json`
- `app/mesh485` — shell + `Mesh485Client` / `FixtureMesh485Client` seam
- `firmware/heltec_v4/scripts/m485_devices.py` — how USB mux actually works (skip non-`{` Companion/boot lines; opening CDC often resets the S3)
- `vendor/meshcore-open` — **library only** (USB serial open / BLE scan+NUS). Do **not** vendor messenger UI, contacts, channels, QR, photos.

## Semantics (must match)

### Live transport

Firmware USB mux: a burst starting with `{` is one MESH 485 NDJSON line (max 512, until `\n` or ~50 ms). Response is one JSON line + `\n`. Everything else on the CDC pipe is MeshCore Companion binary (first byte is never `{`). BLE NUS was **not** muxed in phases 06–12.

- **Windows (required):** USB CDC (bench: **COM23** unless `flutter devices` / serial enum shows another Heltec port). Implement `Mesh485Client` over a serial port: write ASCII/UTF-8 JSON request + `\n`; read lines; ignore lines that do not parse as `m485.response` / `m485.event`. Opening the port may reboot the S3 — wait and skip boot noise like the Python script.
- **Android (required if cheap, else document):** same NDJSON on BLE NUS **or** USB-OTG. If NUS still swallows `{` as Companion, add a **small** BLE mux copied from the USB mux (first byte `{` → MESH 485, do not feed those bytes to Companion). Do not wrap MESH 485 JSON in meshcore-open `wrapUsbSerialTxFrame` (`0x3c`/`0x3e`) — that codec is Companion frames, not this protocol.
- Keep `FixtureMesh485Client` as fallback when no port / no device (Settings shows which transport is active).
- Isolate any copied meshcore-open serial/BLE helper in `app/mesh485` (or a thin `lib/` file). Do not import chat screens.
- LAN REST: skip unless already trivial.

Ops the console must call live (VIEWER): `NODE_STATUS`, `DIAGNOSTICS`, `DEVICE_DISCOVER`, `DEVICE_INFO`, `DEVICE_STATUS`, `POINT_LIST`, `EVENT_LIST`. Optional this run: `EVENT_SUBSCRIBE` on USB for a live Events tail. Do **not** call `BRIDGE_OPEN` / `COMMAND_EXECUTE` / `ACCESS_*` as working operator actions.

Empty PP bus: `PP-001` `online: false`, point `PP` `FAULT`, no invented FIRE. Dual ACK chips stay two bools (`mesh_ok` / `device_ok`).

### Operator screens (same widgets Windows + Android)

Replace fixture-only placeholders with data from the client (live or fallback):

- **Dashboard** — object count, device count, NORMAL / FAULT / ALARM from points, extender online from `NODE_STATUS` / `DIAGNOSTICS`.
- **Objects** — site card listing the three registry devices (C2000-ПП = PP-001; turnstile = TS-001; RAW bus). Signal-10/20 still “later”, not fake adapters.
- **Devices** — Device + Point rows; capabilities `READ_STATUS` only.
- **Events** — list from `EVENT_LIST` (empty `[]` is OK). Show `id`, `device_id`, `type`, `source`, `mesh_ok`, `device_ok`. Filters: device_id / type if cheap.
- **Mesh** — extender health table: node `EXT-001`, RSSI, SNR, RX/TX, uptime, last_seen, radio/repeater. **Not** contacts/chat.
- **Bridge** — still placeholder (`READ_ONLY` / off). Display `NODE_STATUS.session` if live; no operator `BRIDGE_OPEN`.
- **Access** — still `TS-001` card from DEVICE_INFO / points. No employees, no OPEN.
- **Settings** — port picker (Windows COM / Android BLE device), role chip, transport label. Do not expose GPIO maps to edit.

Login may stay a stub role (`viewer` → Russian label). Refresh/poll on an interval is OK; do not block `mesh.loop()` on the node (client-side only).

### Russian UI (this run — all operator apps)

Product language is **Russian**. Translate operator-facing chrome, not the wire protocol.

1. **Flutter `app/mesh485` (Android + Windows + web)**  
   - `flutter gen-l10n` (or equivalent). Default locale **`ru`**.  
   - All shell strings: nav (Дашборд / Объекты / Устройства / События / Меш / Мост / Доступ / Настройки), login, buttons, status words (норма / неисправность / тревога, в сети / не в сети), Bridge/Access placeholders, Settings.  
   - Keep protocol identifiers in Latin: `RAW-001`, `TS-001`, `PP-001`, `EXT-001`, op names, JSON field names. Dual ACK may stay `mesh_ok` / `device_ok` or be labeled «сеть» / «прибор» with the bool visible.  
   - Material widgets, Android `label`, Windows window title, web `<title>`: Russian product name (e.g. «MESH 485» may stay as brand; surrounding chrome in Russian).  
   - Widget tests must follow the RU strings (or look up l10n), not the old English literals.

2. **Heltec OLED (`docs/oled-ui.md`)**  
   - Operator strings in Russian, **short** (128×64, ~21 glyphs at 6×8). Default Adafruit 6×8 has **no Cyrillic** — add a small 6×8/7×10 font that includes `А–Яа–яЁё` (do not pull FreeSans 12 pt).  
   - Suggested (fit the row; shorten if needed): ГОТОВ / ПРИЛОЖИТЕ КАРТУ / ДОСТУП ОК / ОТКАЗ / ДИСТАНЦ. / БЛОКИРОВКА / АВАРИЯ / МЕНЮ / ИНФО / ПОВТОРИТЕЛЬ / НАЗАД / МОСТ. Keep `M●`/`R`, device ids, `PP-001`, baud `M485 9600`, `BR RO` as technical tokens if they overflow.  
   - Update `docs/oled-ui.md` mockups to the strings you actually flash.  
   - Still no MeshCore chat/QR/photos on the panel.

3. **Out of scope for translation**  
   - `vendor/` (MeshCore, meshcore-open UI).  
   - JSON keys / examples under `protocol/mesh485`.  
   - Python lab scripts (optional RU comments only).  
   - Firmware log `printf` / USB debug English is OK.

### Firmware

- Allowed: BLE `{` mux cloned from USB mux; OLED strings + Cyrillic font; compile fixes.  
- Not allowed: pinout changes, EventStore redesign, `COMMAND_EXECUTE`, Path B Orion, NVS protocol persist, AUTO discovery.  
- OLED must still reflect MONITOR / Access states already implemented (READY + PP-001 NORMAL/FAULT/ALARM, GRANT/DENY, bridge session), **not** chat. Live FAULT/ALARM on READY if the adapter already publishes them (empty PP bus stays FAULT).

## Deliverables

1. `app/mesh485` — live `Mesh485Client` (USB Windows required; BLE Android or documented mux/OTG blocker) + fixture fallback; operator screens bound to Device/Point/Event/NODE_STATUS/DIAGNOSTICS.  
2. Russian l10n for Flutter **and** OLED (font + strings).  
3. `docs/apps.md` — live transport + RU UI; CONTROL still prompt 14.  
4. `docs/oled-ui.md` — Russian mockups if OLED strings changed.  
5. `docs/implementation.md` Phase 13 (include `flutter run -d windows` log; Android if a device is present; USB live `DEVICE_DISCOVER` against COM if the node is on the bench).  
6. `AGENTS.md` / `CASCADE.md` / Cursor Next agent file → `14-allowed-control.md`.

## Forbidden this run

- Soldering; GMT147/ST7789; MIFARE UART/1-Wire; motor/relay wiring.
- MeshCore Open UI, chat, contacts, channels, QR.
- Live CONTROL / `COMMAND_EXECUTE` / write coils / fire-disable / operator `BRIDGE_OPEN`.
- Signal-10/20 adapters; Path B Orion UART; wiring `crc8_orion`.
- ACCESS_* 3MAP / offline ACL / employees CRUD / OPEN (17–18).
- Photos, OTA, Excel, AUTO discovery, NVS protocol persist.
- Changing Access GPIO map; forking `vendor/MeshCore`; RS232/ESP-NOW bridge.
- Byte-streaming RAW over LoRa.
- Translating protocol JSON keys or vendoring meshcore-open messenger.

## Done when

1. Windows console can list live `RAW-001` / `TS-001` / `PP-001` from USB CDC when the node is on COM (or documented: port busy / no device → fixture fallback visible in Settings).  
2. Android BLE or OTG path works **or** the BLE-NUS mux gap is documented and fixture/OTG is explicit.  
3. Dashboard / Objects / Devices / Events / Mesh show extender health and dual ACK, not chat.  
4. Flutter + OLED operator strings are Russian; protocol ids stay Latin.  
5. Phase 13 note in `docs/implementation.md`. Next cascade file: `agent/prompts/14-allowed-control.md`.

Then stop.
