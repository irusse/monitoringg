# ADR 0003 — Photos never on LoRa

## Status

Accepted

## Context

Employee records include photos. MeshCore packets are small and the mesh is a control plane.

## Decision

Photos live in server object storage and Flutter cache. Controllers store `employee_id`, UID, permissions, schedule, validity only.

## Consequences

Offline turnstile still works. Phone shows photo when talking to the server, not when only on LoRa.
