# Isolated RS-485 on Heltec V4

MVP: **one** isolated transceiver. Do not use a bare MAX485 without galvanic isolation on a real fire/security bus.

```
Heltec 3.3 V UART1  →  isolated RS-485  →  A / B  (+ shared GND/ISO as the module requires)
```

DE/RE driven from GPIO 26. Fail-safe: DE low (receive) except during TX. Listen-only tap on a С2000-М Orion bus keeps DE at RX so Heltec is not a second master (`docs/c2000m.md`).

## Pins (frozen Phase 05)

Access/Wiegand/OLED/USB/LoRa stay as in `docs/pinout.md`. GNSS module is **not** fitted.

| Signal | GPIO | Why |
|---|---|---|
| RS485 TX | **39** | silk GNSS_TX, UART1 TX |
| RS485 RX | **38** | silk GNSS_RX, UART1 RX |
| RS485 DE/RE | **26** | header; HIGH = transmit |

Firmware: `Rs485Hal` (`include/hal/rs485.h`), default **9600 8N1**. MeshCore `ENV_INCLUDE_GPS=0` so companion GPS does not open Serial1 on these pins.

If a GNSS module is required on the same board, move UART to verified-free header pins and document the exception. Do not assume GPIO 47/48 are free until PSRAM/flash occupancy is checked on the exact V4 module.

## Electrical

- Common **signal** ground only as the isolator datasheet allows; never share turnstile motor return with A/B.
- 120 Ω termination if the bus needs it (usually at the ends, not on every spur).
- Failsafe bias per transceiver.
- 12 V / 24 V bus power is **not** taken from Heltec 5 V.

BOM: isolated RS-485 module (ISO3082-class or similar), pluggable A/B.

## Loopback test (Phase 05)

```
Heltec TX/RX/DE  ↔  isolated RS-485  ↔  A/B  ↔  USB-RS485 dongle  ↔  PC (9600 8N1)
```

Firmware `Rs485Hal`: 9600 8N1 default, configurable baud/parity/stop, `writeFrame` / `read` / `readFrame(timeout)` (stop on inter-byte gap after first RX, else timeout), DE around TX + flush. Idle RX is shown on OLED (`RS485 N B`) and is **not** echoed on the bus. TX is MESH 485 `RS485_TRANSACTION` (phase 06). USB CDC (COM23) stays MeshCore companion — JSON `{...}` lines are muxed; do not dump NDJSON onto companion frames.

Default 9600 8N1. MeshCore `loop()` keeps running. Phase 07: a `READ_ONLY` bridge session captures idle RX into a RAM ring (`BRIDGE_POLL`); DE stays RX except during an explicit `RAW_BRIDGE` `RS485_TRANSACTION`.
