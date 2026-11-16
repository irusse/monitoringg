# Competitive landscape — MESH 485 Extender

Research date: 2026-08-28. Direct combo **MeshCore + isolated RS-485 + Bolid adapters + native Android/Windows console** was not found. Nearby products fall into three classes.

## Comparison

| Project / product | Type | RS-485 bridge | Wireless | Protocols | Operator UI | Open source | Use for us |
|---|---|---|---|---|---|---|---|
| **MESH 485 Extender** | this repo | yes | MeshCore LoRa + BLE/Wi-Fi | Bolid, Modbus, RAW, Access | native Android/Windows | yes | target |
| [hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) | HA integration | via С2000-ПП + HUB-C2000PP | Ethernet/Wi-Fi | Orion mapped in HUB, not UART in HA | Home Assistant | Apache-2.0 | ⭐ zone/relay/section/event model |
| Bolid С2000-ПП | commercial converter | yes | no | Orion ↔ Modbus RTU | via SCADA/HUB | no | ⭐ official integration path |
| Bolid С2000-Ethernet | commercial | yes | Ethernet | Orion RS-485/RS-232 | PC software | no | wired analogue of our bridge |
| Bolid С2000-WiFi | commercial | yes | Wi-Fi | Orion | vendor | no | wireless interface idea |
| Bolid С2000-РПИ | commercial radio | yes | proprietary radio | Orion RS-485 | vendor | no | closest **radio extender** concept |
| Bolid + Спектр-433 tests | vendor application note | transparent RS-485 | radio modem | Orion | n/a | no | timings (~50 ms / 800–900 ms), proves radio RS-485 is used in the field |
| [zivillian/esp32-modbus-gateway](https://github.com/zivillian/esp32-modbus-gateway) | firmware | yes | Wi-Fi/Ethernet | Modbus RTU/TCP | web | GPL-3.0 | ⭐ UART/DE/RE, config, tests |
| [vwetter/esp32-modbus-gateway](https://github.com/vwetter/esp32-modbus-gateway) | firmware | yes | Wi-Fi | Modbus RTU/TCP | web/REST/OTA | yes | REST/OTA/web diagnostics ideas |
| ElectroSoul ESP32 RS-485 Gateway | firmware+HW | yes | Wi-Fi | RS-485 / Modbus | dashboard | GPL-3.0 | industrial RS-485 hardware |
| ModQ | firmware | yes | Wi-Fi/MQTT | Modbus, profiles | WebUI | yes | device-profile idea |
| esp-modbus-mqtt | firmware | yes | Wi-Fi/MQTT | Modbus RTU | MQTT | GPL | event bus towards HA/Grafana |
| Milesight UC300 / UC100 | commercial | yes | LoRaWAN / 4G | Modbus + transparent RS-485 | cloud/web | no | ⭐ closest IoT analogue (LoRaWAN, not mesh) |
| Dragino RS485-LB/LS / LN | commercial | yes | LoRaWAN | Modbus / RS-485, downlink to bus | BLE config, cloud | no | RS-485 → LoRa + OTA ideas |
| MeshCore | stack | via our adapters | LoRa mesh | companion protocol | official apps | MIT | ⭐ transport only |
| [ALLFATHER-BV/meshcomod](https://github.com/ALLFATHER-BV/meshcomod) | MeshCore fork | n/a | USB+BLE+TCP+WS | companion | same family | see repo | ideas for multi-interface; do not replace upstream |

## Three classes

1. **Bolid** — know Orion, closed, not MeshCore, not a plugin adapter framework.
2. **Industrial IoT (Milesight, Dragino)** — RS-485 + LoRaWAN + often Modbus/cloud. No autonomous multi-hop mesh, no Bolid native.
3. **DIY ESP32 gateways** — solid RS-485/Modbus/API, no MeshCore, no Bolid abstraction.

## Gap we occupy

Open-source **adapter framework** + **MeshCore multi-hop** + **Bolid** (Modbus-via-ПП and/or native monitor) + **local Android/Windows console** + **Access profile** on the same Heltec V4.

## Licensing note

GPL-3 ESP32 Modbus gateways: copy **ideas** (HAL, web diagnostics), not code, unless we accept GPL on the firmware. hass-hubc2000pp Apache-2.0 is compatible as a reference for models. MeshCore stays unmodified vendor.
