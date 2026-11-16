# ADR 0002 — Hidden MeshCore + disableable repeater on the same MCU

## Status

Accepted

## Context

Heltec V4 should be an Access controller **and** a MESH 485 RS-485 gateway that also forms a MeshCore network. Users must not see MeshCore chat UI. Repeater must be switchable.

## Decision

Variant A: MeshCore + Access on one ESP32-S3.

- Companion example is the host interface base (BLE/USB/Wi-Fi).
- Repeater forwarding copied behind `repeater_enabled`.
- OLED is Access UI + `M●` indicator.
- Diagnostics mesh screen only via long-press menu.

## Consequences

Need careful loop timing so Wiegand interrupts and LoRa TX do not starve each other. If this fails in the field, reopen Variant B (UART companion radio).
