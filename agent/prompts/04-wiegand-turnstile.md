# PROMPT 04 — Wiegand + turnstile + lamp

Use `vendor/esp32-rfid` + `vendor/WiegandNG`. Pins only from `docs/pinout.md`.

- Wiegand 26/34 → `CardEvent`
- Relay OPEN pulse (esp32-rfid `door.esp`)
- Green/red lamps
- ENTRY/EXIT/EMERGENCY
- State machine LOCKED / OPENING / OPEN / CLOSING / ERROR / EMERGENCY

On card/open/deny **update the OLED** (`showGranted` / `showDenied`), not only Serial.

Level shifter required. ACL can still be stub.
