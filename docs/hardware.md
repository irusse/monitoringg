# Hardware

See canonical **[docs/pinout.md](pinout.md)**.

GPIO **19/20 = USB** on ESP32-S3. Do not wire Wiegand or sensors there.

ESP32-RFID / PiDoors GPIO numbers are for other boards. Copy **circuits** (shifter, opto relay, lamps), not pin numbers.

MVP I/O: D0=15 D1=16 OPEN=40 GREEN=41 RED=33 ENTRY=3 EXIT=4 ESTOP=6. OLED 17/18/21, VEXT 36 **LOW = on**, USER 0. USB 19/20 free. Isolated RS-485 TX/RX/DE = 39/38/26 (`hardware/rs485-adapter.md`).
