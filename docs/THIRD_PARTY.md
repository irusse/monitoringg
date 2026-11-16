# Third-party sources (MESH 485 Extender)

| Source | Role | Do not |
|---|---|---|
| [MeshCore](https://github.com/meshcore-dev/MeshCore) | LoRa mesh, Companion, Repeater | rewrite protocol; put chat on OLED |
| [hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) Apache-2.0 | Bolid entity/event mapping | use as RS-485 parser; require HUB on the ESP32 |
| Official Bolid / С2000-ПП docs | frames, Modbus, timings, allowed ops | reverse-engineer as the only source |
| CXEM Orion thread | dumps, CRC tables as tests | vendor decrypted firmware |
| [meshcomod](https://github.com/ALLFATHER-BV/meshcomod) | multi-interface ideas | replace MeshCore upstream |
| Milesight / Dragino / С2000-РПИ | product analogues | copy closed firmware |
| ESP32 Modbus gateways | HAL/UI ideas | paste GPL code without ADR |

Full tree: `NOTICE.md`, `docs/vendor-map.md`, `docs/competitive.md`.
