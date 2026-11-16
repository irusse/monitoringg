# FIRST RUN — historical (phase 01, already executed)

Do **not** paste this as the next agent message. Phases 01–04 are done. Next: `agent/prompts/05-rs485-hal.md`.

---

Original text (Access pinout + OLED). Product is now MESH 485 Extender; this run remains the Access/OLED foundation.

You are implementing **3MESH Access** in this repo. This is **run 1 of 13**. Do only this run. Stop when the OLED Access UI is on the Heltec V4 screen and pinout is frozen.

## Product (do not expand scope)

Heltec WiFi LoRa 32 V4 = autonomous turnstile controller + **hidden** MeshCore (BLE+USB companion, optional repeater).

Android and Windows = **one** Access app (turnstiles, events, employees). **No MeshCore nodes/chat** in that app.

This run is **firmware display + pinout only**. No Flutter, no server, no Wiegand logic, no MeshCore chat UI.

## Read first

- `AGENTS.md`
- `docs/pinout.md`
- `docs/oled-ui.md`
- `docs/architecture.md`
- MeshCore OLED: `vendor/MeshCore/src/helpers/ui/SSD1306Display.h` (128×64, `PIN_OLED_RESET`, SDA 17 / SCL 18)
- `vendor/MeshCore/variants/heltec_v4/platformio.ini` section `heltec_v4_oled`
- `firmware/heltec_v4/` existing stubs

## Rules

- GPIO **19/20 = USB PHY**. Never Access I/O.
- Do not reuse LoRa/PA/OLED pins for Wiegand/relay.
- Do not fork `vendor/MeshCore`.
- Do not draw MeshCore companion `ui-new` screens. Access UI only + `M●`/`M○`/`M!`.

## Deliverables

### A. Pinout freeze

Confirm and write into `docs/pinout.md` + `firmware/heltec_v4/include/access/pins.h` + `hardware/wiring.md`:

| Signal | GPIO |
|---|---|
| Wiegand D0 / D1 | 15 / 16 |
| Relay OPEN | 40 |
| Lamp G / R | 41 / 33 |
| ENTRY / EXIT / E-STOP | 3 / 4 / 6 |
| OLED | 17 / 18 / 21 |
| USER | 0 |

If silkscreen contradicts, document the change. Keep USB 19/20 free.

### B. OLED Access interface on Heltec V4

Implement real SSD1306 output (not only `Serial.printf` fakes).

Use MeshCore `SSD1306Display` **or** Adafruit_SSD1306 on Wire 17/18/RST 21. `Vext`/periph power as on Heltec V4 (`PIN_VEXT_EN`) so the panel actually lights.

Screens from `docs/oled-ui.md`:

1. **Idle:** device id, `READY`, `Present card`, user count, `M●` (stub radio = active if you have not started MeshCore yet).
2. **Granted / Denied / Remote / Locked** — callable from `AccessDisplay` (demo: cycle with USER short press or a 4 s demo loop behind `#ifdef OLED_DEMO`).
3. Long-press USER → **MENU** (Info / Back). No chat.

Files: `firmware/heltec_v4/src/display/access_display.cpp` and headers. `main.cpp` calls `oled.begin()` and `showReady("TS-001", ...)`.

PlatformIO: Heltec V4 OLED flags from MeshCore (`HELTEC_LORA_V4_OLED`, `DISPLAY_CLASS=SSD1306Display` if you link MeshCore; otherwise Adafruit_SSD1306 + same pins). Document `pio run` / flash in `firmware/heltec_v4/README.md`.

## Done when

1. Pinout files match the table (or a written exception).
2. After flash, the **physical OLED** shows the Access READY layout, not MeshCore splash/chat.
3. USER button changes screen (demo cycle or menu).
4. Short note in `docs/implementation.md` under Phase 01.

Then stop. Next cascade item is `agent/prompts/02-firmware-ble-usb.md`.
