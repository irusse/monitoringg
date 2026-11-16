# Pinout — Heltec WiFi LoRa 32 V4 (OLED)

Canonical wiring for the Access node. Confirm on silkscreen before soldering.

Sources: [MeshCore Heltec V4](https://github.com/meshcore-dev/MeshCore), [ESP32-RFID pin table](https://github.com/pvtex/esp32-rfid), [PiDoors Wiegand wiring](https://github.com/sybethiesant/pidoors).

## Do not use

| GPIO | Why |
|---|---|
| 0 | BOOT / USER — Access menu only, not Wiegand |
| 1 | VBAT ADC |
| 2, 5, 7, 46 | LoRa PA / FEM |
| 8–14 | SX1262 SPI / DIO / RESET / BUSY |
| 17, 18, 21 | OLED I2C (SDA / SCL / RST) — not Access I/O |
| **19, 20** | **ESP32-S3 USB D− / D+** (BLE+USB companion needs these) |
| 34, 42 | GPS EN / RST (module unused; RX/TX taken by RS-485) |
| **38, 39** | **RS-485 RX / TX** (silk GNSS_RX / GNSS_TX) |
| 35 | TX LED |
| 36, 37 | VEXT (OLED power) / ADC ctrl |
| 43, 44 | UART0 |
| 45 | S3 strapping |

ESP32-RFID generic D0=4 D1=5 **cannot** be copied: on V4 GPIO 5 is PA CTX.

PiDoors Pi GPIO 23/24 also cannot be copied. Only the **principle**: D0/D1 + 5 V→3.3 V shifter + opto relay + green/red lamp.

## Access map (OLED V4) — frozen Phase 01

Firmware: `firmware/heltec_v4/include/access/pins.h`. Wiring: `hardware/wiring.md`.

| Signal | GPIO | Direction | Notes |
|---|---|---|---|
| Wiegand D0 | **15** | IN | level shift 5 V → 3.3 V |
| Wiegand D1 | **16** | IN | same |
| Relay OPEN | **40** | OUT | pulse to turnstile OPEN, optocoupler |
| Lamp green | **41** | OUT | MOSFET, 12 V lamp |
| Lamp red | **33** | OUT | deny / locked |
| Sensor ENTRY | **3** | IN_PULLUP | dry contact to GND |
| Sensor EXIT | **4** | IN_PULLUP | dry contact to GND |
| EMERGENCY | **6** | IN_PULLUP | press to GND (active LOW). Unconnected = idle. |
| OLED SDA | **17** | I2C | silk `SDA_OLED`; not Arduino `SDA` |
| OLED SCL | **18** | I2C | silk `SCL_OLED`; not Arduino `SCL` |
| OLED RST | **21** | OUT | silk `RST_OLED` |
| VEXT EN | **36** | OUT | **LOW** = OLED / periph rail on (PMOS). MeshCore `PIN_VEXT_EN_ACTIVE=HIGH` is inverted — do not drive HIGH. |
| Buzzer (opt) | unused | — | use lamp; GPIO short |
| USER | **0** | IN | Access menu, not MeshCore UI |
| USB D− / D+ | **19 / 20** | USB PHY | never Access I/O |
| RS-485 TX | **39** | OUT | UART1 TX, silk `GNSS_TX`. No GPS module. |
| RS-485 RX | **38** | IN | UART1 RX, silk `GNSS_RX` |
| RS-485 DE/RE | **26** | OUT | HIGH = TX, LOW = RX (fail-safe) |

### Silkscreen vs this map

Heltec V4 / MeshCore `pins_arduino.h` labels Arduino `SDA=3` and `SCL=4` (env/GPS I2C). The OLED is on dedicated silk **SDA_OLED=17 / SCL_OLED=18 / RST_OLED=21**. Access **ENTRY/EXIT use 3/4**, so firmware must call `Wire.begin(17, 18)` and must not use default Wire pins. USB 19/20 stay free. No Access signal was moved off the FIRST_RUN table.

If GPIO 3/4 are taken by an I2C expander, move sensors to 47/48 (verify they are broken out on your V4).

Isolated RS-485 uses UART1 on GNSS header pins (38/39) + DE 26. A second RS-485 port needs an expander — never USB 19/20 or LoRa SPI. GNSS receiver cannot be fitted at the same time (`ENV_INCLUDE_GPS=0`).

## EM-Marine 125 kHz

External Wiegand 26/34 reader. Heltec does not read 125 kHz.

```
Reader +12V ── reader PSU
D0 / D1 ── shifter HV ── LV ── GPIO 15 / 16
GND ── common GND (not motor return)
LED/Buzzer of reader (opt) ── shifter ── GPIO 41 / extra
```

PiDoors warning applies: 5 V Wiegand without shifter kills 3.3 V pins over time.

## Turnstile

```
GPIO 40 ── opto relay ── turnstile controller OPEN (dry contact)
GPIO 3/4 ── pass sensors
GPIO 6 ── e-stop
GPIO 41/33 ── green/red 12 V lamps via MOSFET
```

## Host ports (firmware)

One binary: **BLE + USB** MeshCore Companion (`MultiSerialInterface` can register both).

| Port | Use |
|---|---|
| USB-C CDC | Access app (Windows), meshcore-cli, flash |
| BLE NUS | Access app (Android), MeshCore radio tools |
| LoRa | hidden repeater + MESH 485 between nodes + remote repeater CLI |
| Wi-Fi | optional LAN REST / OTA / photo-less ACL sync |

MESH 485 **application** uses BLE/USB as a pipe to this node (devices, events, bridge, Access). Repeater identity/radio can use MeshCore tools; the operator console may show extender node health, not MeshCore chat.

## RS-485 (Phase 05 — frozen)

Isolated transceiver on a live Orion bus. GNSS header UART is the RS-485 port (no GPS module).

| Signal | GPIO | Notes |
|---|---|---|
| RS485 TX | **39** | silk GNSS_TX, UART1 |
| RS485 RX | **38** | silk GNSS_RX, UART1 |
| RS485 DE/RE | **26** | HIGH only while transmitting |

Details: `hardware/rs485-adapter.md`. A second RS-485 port is a later expander — not USB 19/20.

**С2000-М:** connect the panel’s **Orion RS-485 A/B** through the isolator (Path A via С2000-ПП, or listen-only tap with DE = RX). The panel’s **RS-232** (ПК / Орион Про) is not a Heltec pin: ±12 V kills the S3; 19/20 are USB PHY. Events go to MESH 485 `EventStore`, not MeshCore private chat. Instruction: [`c2000m.md`](c2000m.md).

## ПЦН «Тревога» and RDM6300 (two nodes, one image)

Same firmware `heltec_v4_access`. The panel node and the monitor/ACS node do not share UART1. Confirm GPIO 47 on the V4 silkscreen before soldering (`47/48` are the spare pair). Test steps: [`test-pcn-acs.md`](test-pcn-acs.md).

### Panel node — dry contact only

The alarm is the relay the panel already closes for the monitoring station, through an optocoupler. It is not the Orion A/B pair and not RS-232.

| Signal | GPIO | Notes |
|---|---|---|
| ПЦН «Тревога» | **47** | `INPUT_PULLUP`, active LOW via opto transistor to GND. Open input = normal, not alarm |
| RS-485 | **39 / 38 / 26** | Leave for a later listen-only tap. This track does not claim UART1 |
| RDM6300 | — | Not on this node |

Opto, panel side: panel supply — series resistor — opto LED — relay contact — panel ground. Heltec side: opto collector to GPIO 47, emitter to Heltec GND. Do not land 12 V or a relay coil on the pin.

### Monitor node — RDM6300 and turnstile

UART1 RX is the reader. Do not fit the Orion isolator on this board.

| Signal | GPIO | Notes |
|---|---|---|
| RDM6300 TX | **38** | UART1 RX, 9600 8N1, only when the image is built with `-D MESH485_RDM6300=1`. 5 V TX through a divider (2.2 kΩ to the pin, 3.3 kΩ pin to GND). Measure idle before leaving it connected |
| RDM6300 RX | — | Leave open |
| RDM6300 VCC | 5 V pin | Module supply, not a GPIO. GND common with Heltec |
| DE | **26** | Hold RX (LOW) if a transceiver is still populated |
| Wiegand 15/16 | unused | RDM6300 mode does not also arm D0/D1 |
| OPEN / lamps / sensors | **40 / 41 / 33 / 3 / 4 / 6** | Unchanged Access map |
