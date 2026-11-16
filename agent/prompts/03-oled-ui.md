# PROMPT 03 — OLED Access UI (Heltec V4)

The operator interface on the **device screen** is required, not optional.

Display: SSD1306 128×64. Spec: `docs/oled-ui.md`. Driver: MeshCore `SSD1306Display` (already 128×64, pins 17/18/21). Enable VEXT so the panel is powered.

Implement `AccessDisplay` for all screens:

- Idle READY + `M●`/`M○`/`M!` + optional `R` if repeater on
- ACCESS GRANTED (name, direction, time)
- ACCESS DENIED (reason, time)
- REMOTE OPEN (operator)
- LOCKED / EMERGENCY
- Long-press USER → MENU (Info, repeater on/off if allowed, Back)

Wire later events in prompt 04–05; for now call screens from:

- boot → idle
- USER short press → demo cycle if no RFID yet
- USER long press → menu

**Forbidden:** MeshCore chat, contacts, LoRa splash as home screen, photos, QR.

If run 1 already drew READY, complete remaining screens and replace `Serial.printf` stubs in `access_display.cpp`.

Update `docs/oled-ui.md` if you change layout. Note in `docs/implementation.md`.
