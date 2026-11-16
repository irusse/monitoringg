# Agent rules

MESH 485 Extender on Heltec V4. MeshCore is transport. Application layer is MESH 485.

1. Do not write LoRa mesh. Use `vendor/MeshCore`. Do not fork it. `meshcomod` is ideas only.
2. Firmware: BLE **and** USB companion in one image. GPIO 19/20 are USB — never RS-485, Wiegand, or sensors. Pinout: `docs/pinout.md`.
3. RS-485 is a HAL with configurable baud/parity/stop and DE/RE. Prefer an **isolated** transceiver. Transport **transactions**, not UART bytes, over MeshCore.
4. Device logic lives in adapters (`DeviceAdapter`). Clients see Device / Point / Event — never raw Bolid opcodes.
5. Bolid: take **models and events** from `hass-hubc2000pp` (Apache-2.0). That repo talks to HUB-C2000PP over IP — it is **not** an Orion UART parser. Wire protocol = official Bolid docs + verified dumps. CXEM dumps are test vectors, not firmware source. Do not paste decrypted vendor firmware.
6. First Bolid path: **read-only**. Prefer official **С2000-ПП Modbus RTU** when the converter is on the bus. Native Orion CONTROL is later and capability-gated.
7. Fire commands must not be a single remote “disable alarm”. Default mode READ_ONLY. Loss of MeshCore must not stop the panel.
8. Access/turnstile is `PROFILE_ACCESS` on this image: Wiegand from `vendor/esp32-rfid` + `vendor/WiegandNG`, not their GPIO numbers. Offline ACL still works if the server is down.
9. Flutter app is one console (Android = Windows): objects, devices, events, extender mesh **status**, bridge, Access module. **No** MeshCore chat, contacts, or channels. Extract companion codec from `vendor/meshcore-open` library only.
10. Heltec SSD1306 shows product UI (`docs/oled-ui.md`), not MeshCore screens.
11. Photos never on LoRa or OLED.
12. Cascade **01–19 complete**. Phases **20–23** and **25–28** done. **Do not assign a next prompt** until the operator chooses between the remaining listen-only job `agent/prompts/24-c2000m-monitor.md` and a stop. Phase 28 checked PCN and the RDM6300 ACL together by fixture (`docs/report-28-ru.md`); live boards were not run. The monitor image still reads RDM6300 (`-D MESH485_RDM6300=1`, GPIO 38) into `TS-001`. No seventh DISCOVER id. Leftovers: AUTO discovery, dual UART, Postgres/FastAPI photos CRUD, OTA, native Orion CONTROL, MeshCore chat/QR. Wiring: `docs/c2000m.md`, `docs/pinout.md`, bench `docs/test-pcn-acs.md`.
13. Do not modify `vendor/` without ADR.
