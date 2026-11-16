# ADR 0004 — Access app is not MeshCore Open

## Status

Accepted

## Decision

Operator UI is a new Flutter app with PiDoors-like СКУД screens. MeshCore Open / official MeshCore remain radio/repeater tools (BLE+USB). Firmware exposes both BLE and USB Companion on one binary.

## Consequences

MESH 485 operator console **may** list extender nodes (RSSI/online) per ADR 0005. It still must not embed MeshCore chat, contacts, or channels. Access screens have no messenger UI.

Slight duplication of companion codec (copy library, not UI).
