# Wiring

Canonical pins: `docs/pinout.md` (frozen Phase 01). Firmware: `firmware/heltec_v4/include/access/pins.h`.

```
EM-Marine 12 V reader
  D0 -- 5V-3V3 shifter --> Heltec GPIO 15
  D1 -- shifter ---------> Heltec GPIO 16
  GND -------------------- Heltec GND
  +12 -------------------- reader PSU only

Opto relay
  IN  <-- GPIO 40
  NO/COM --> turnstile OPEN

Lamps 12 V via MOSFET
  Green <-- GPIO 41
  Red   <-- GPIO 33

Dry contacts to GND (INPUT_PULLUP)
  ENTRY --> GPIO 3
  EXIT  --> GPIO 4
  E-STOP to GND when pressed (GPIO 6, active LOW)

On-board OLED (do not rewire)
  SDA_OLED --> GPIO 17
  SCL_OLED --> GPIO 18
  RST_OLED --> GPIO 21
  Vext     --> GPIO 36 (LOW = panel power, PMOS)

USER / PRG --> GPIO 0 (Access menu)

Isolated RS-485 UART1 (no GPS module)
  Heltec GPIO 39 TX --> isolator DI
  Heltec GPIO 38 RX <-- isolator RO
  Heltec GPIO 26 DE --> isolator DE+/RE- (HIGH = TX; listen-only: keep RX)
  Heltec GND -------- isolator logic GND
  isolator A/B ------ field bus (С2000-ПП Path A, or listen-only tap on С2000-М Orion A/B)

С2000-М RS-232 (±12 V) is not a Heltec pin. GPIO 19/20 = USB. Full instruction: `docs/c2000m.md`.

ПЦН «Тревога» (panel node only) — dry contact through an optocoupler, not A/B:

```
Panel supply -- resistor -- opto LED -- PCN contact -- panel GND
GPIO 47 (pull-up) -- opto collector
Heltec GND -------- opto emitter
```

Open GPIO 47 = normal. Confirm the pin is broken out on the V4 header.

RDM6300 (monitor node only; UART1 is not the Orion bus on this board):

```
RDM6300 VCC -- 5 V
RDM6300 GND -- Heltec GND
RDM6300 TX  -- 2.2k -- GPIO 38
GPIO 38     -- 3.3k -- GND
RDM6300 RX  -- open
```

Turnstile OPEN/lamps/sensors stay on 40 / 41 / 33 / 3 / 4 / 6.

Heltec USB-C --> Windows console / meshcore-cli / flash (this workspace: COM23)
Heltec BLE  --> Android console / MeshCore radio app
Antenna on before any LoRa TX
```

Do not use GPIO 19/20 (USB PHY). Do not put Wiegand or sensors on 17/18/21 (OLED) or 3/4 if you also start default Wire (Access uses 17/18 for the panel).
