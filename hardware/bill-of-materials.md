# Bill of materials (MVP)

| Qty | Item |
|---|---|
| 1 | Heltec WiFi LoRa 32 V4 (OLED, 16 MB) |
| 1 | LoRa antenna for region (EU868 / RU864 / etc.) |
| 1 | EM-Marine 125 kHz Wiegand 26/34 reader |
| 1 | Wiegand 5 V to 3.3 V level shifter or isolator |
| 1 | 1–2 channel opto relay module |
| 2 | MOSFET modules or transistor boards for 12 V lamps |
| 1 | 12 V PSU for reader + lamp + turnstile |
| 1 | 5 V for Heltec (USB or regulator from 12 V) |
| 3 | Dry-contact sensors or turnstile pass outputs |
| 1 | E-stop button NC |
| 1 | Isolated RS-485 transceiver (ISO3082-class or module, A/B + DE) |
| 1 | USB-RS485 dongle (bench loopback for prompt 05) |
| optional | Second isolated RS-485 (later dual-port / Bolid + ACS split) |
| optional | Bolid С2000-ПП (preferred official Modbus path) |
| optional | RTC DS3231 on free I2C (GPIO 3/4 expansion) |
| optional | microSD (only if SPI pins are free; V4 LoRa already uses SPI — use a second SPI or I2C FRAM instead) |

microSD on the same SPI as SX1262 is **not** recommended. Prefer LittleFS on 16 MB flash for events; archive via Wi-Fi sync.
