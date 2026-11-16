# Vendor map (what we actually use)

## Keep and copy from

### [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore) — `vendor/MeshCore`

Use as library: companion BLE+USB + optional repeater flag. Do not fork. Do not show chat UI on OLED or in the operator console.

### [Regressor/hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) — `vendor/hass-hubc2000pp` (cloned prompt 09)

Apache-2.0. **Device/event model** (zones, relays, sections, poll + push). Speaks to **HUB-C2000PP over IP**, not Orion UART. Do not copy the Home Assistant component into firmware. Names: `docs/reference/hubc2000pp-entities.md`.

### Official Bolid / С2000-ПП manuals

Registers, Orion host parameters, allowed operations. Source of truth for Path A (Modbus).

### CXEM Orion topic

Test vectors and CRC discussion only. Not firmware source.

### [ALLFATHER-BV/meshcomod](https://github.com/ALLFATHER-BV/meshcomod)

Notes for USB+BLE+TCP on Heltec V4. Do not replace MeshCore upstream.

### ESP32 Modbus gateways (zivillian, vwetter, ElectroSoul, ModQ)

**Ideas** for UART/DE/RE, web diagnostics, device profiles. Many are GPL-3 — do not paste their code into this MIT/Apache mix without a license ADR.

### [pvtex/esp32-rfid](https://github.com/pvtex/esp32-rfid) — `vendor/esp32-rfid`

WiegandNG usage, door pulse, LittleFS users. Not their GPIO table.

Wiegand lib: `vendor/WiegandNG`.

### [sybethiesant/pidoors](https://github.com/sybethiesant/pidoors) — `vendor/pidoors`

Access screens and schema. Not PHP.

### [HAPRS](https://github.com/rahul-chatterjee-27/HAPRS) — `vendor/HAPRS`

Log columns + UUID. Not ESP-NOW.

### meshcore-open / meshcore-client

Companion codec only. Product UI is the MESH 485 Flutter console.

### meshcore-cli / meshcore.js

Technician CLI and optional gateway. Not the operator messenger.
