# NOTICE — third-party software

This project reuses existing open-source components. Do not strip copyright
headers. Before shipping a binary, include this file and the original licenses.

| Component | Path | License | Role |
|---|---|---|---|
| MeshCore | `vendor/MeshCore` | MIT (Scott Powell / rippleradios.com) | LoRa mesh, routing, companion, repeater |
| ESP32-RFID | `vendor/esp32-rfid` | MIT (esp-rfid Community / Ömer Şiar Baysal) | Wiegand, ACL, Web/MQTT/NTP |
| ESP-RFID | `vendor/esp-rfid` | MIT | Original access-control Web UI / reader logic |
| EEH ESP32 RFID | `vendor/eeh-esp32-rfid` | see repo | Offline user list, REST API, OTA, LCD ideas |
| HAPRS | `vendor/HAPRS` | MIT | Local CSV/SD event log, dashboard ideas |
| MeshCore Open | `vendor/meshcore-open` | MIT (zjs81) | Flutter BLE/USB/TCP companion client |
| Offband MeshCore | `vendor/meshcore-client` | MIT (fork of meshcore-open) | Windows + Android companion client |
| MeshCore.js | `vendor/meshcore.js` | MIT | Node/browser companion protocol |
| meshcore-cli | `vendor/meshcore-cli` | see repo | Python CLI / gateway helper |
| PiDoors | `vendor/pidoors` | see repo | Server UI ideas: employees, cards, logs, schedules |
| ESP32 RFID Access Control | `vendor/esp32-rfid-access-control` | see repo | ESP-IDF / FreeRTOS structure reference |
| WiegandNG | `vendor/WiegandNG` | see repo (used by ESP32-RFID) | Multi-bit Wiegand decoder |
| hass-hubc2000pp | `vendor/hass-hubc2000pp` | Apache-2.0 | Bolid zone/relay/section/event **model** via HUB-C2000PP; not an Orion UART parser |

MeshCore companion protocol is documented in:

`vendor/MeshCore/docs/companion_protocol.md`

Do not rewrite MeshCore, Companion Protocol, or Wiegand decoder from scratch.
