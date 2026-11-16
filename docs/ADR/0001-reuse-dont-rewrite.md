# ADR 0001 — Reuse MeshCore, ESP32-RFID, Flutter client

## Status

Accepted

## Context

Need a production-oriented turnstile controller with LoRa mesh, Wiegand, Android and Windows apps.

## Decision

Compose existing MIT (and similarly licensed) projects. Own code is Access HAL, 3MAP, sync, employee server, Flutter Access module (~20%).

## Consequences

Faster path to MVP. Must track MeshCore API changes. Must keep vendor trees unmodified.
