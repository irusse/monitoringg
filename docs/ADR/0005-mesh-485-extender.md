# ADR 0005 — MESH 485 Extender (platform) + Access profile

## Status

Accepted

## Context

Phases 01–04 delivered a Heltec V4 turnstile HAL with hidden MeshCore. The product goal widened: the same MCU should be a **universal RS-485 gateway** (Bolid first, then Modbus and others) and still run the turnstile.

A Bolid-only firmware would lock MeshCore to Orion opcodes and block meters, PLC, and other ACS.

## Decision

1. Rename the product to **MESH 485 Extender**. Repository folder may stay `turniket`.
2. Application protocol is **MESH 485**, not “C2000 commands on LoRa”.
3. Instruments plug in as `DeviceAdapter` implementations.
4. Access/turnstile remains `PROFILE_ACCESS` on the same binary (existing Wiegand/OLED work).
5. Default RS-485 privilege is MONITOR / READ_ONLY.
6. The Flutter console **may** list extender nodes (RSSI, online) because they are gateways. It still **must not** embed MeshCore chat. This amends ADR 0004 for the MESH 485 console only; Access screens still have no chat.

## Consequences

- Pinout grows an isolated RS-485 UART without stealing USB 19/20 or OLED/LoRa.
- Cascade after 04 is RS-485 → protocol → bridge → adapters → app → Access ACL, not Access-only 05–13.
- Native Orion CONTROL and AUTO discovery are late.
- Fire isolation is a product requirement, not an afterthought.
