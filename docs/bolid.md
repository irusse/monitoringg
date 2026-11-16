# Bolid / Orion

## What we will and will not do

This node is an **integration gateway**. The fire/security panel must keep working if Heltec, LoRa, or the app die.

Default: **MONITOR / READ_ONLY**. CONTROL only for advertised capabilities, with operator role (VIEWER / OPERATOR / ADMIN), extra UI confirmation, and audit (who / when / node / device / command / both ACKs).

There is no MESH 485 op `DISABLE_FIRE_ALARM` and no `DISARM_ALL`. Those strings are hard-refused even if a client sends them.

## CONTROL allow-list (prompt 14 — implemented)

| Item | Rule |
|---|---|
| Envelope `op` | `COMMAND_EXECUTE` |
| Role | `operator` / `OPERATOR` or `admin` / `ADMIN`. `viewer` → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`) |
| Capability | Payload `capability` (alias inner `op`) must be on `Device.capabilities` **and** in the executable set |
| Executable this run | **`CONTROL_RELAY` only** |
| Named point | `R1`…`R8` (relay №1…8). Not a list, not “all relays” |
| Action | `ON` or `OFF` |
| Panel config | RAM allow-list, default **none**. Lab: `C2000ppModbusAdapter::allowRelay(n, true)` or `-D MESH485_ALLOW_PP_RELAY=1` (enables `R1`) |
| Modbus write | FC **0x05** Write Single Coil, coil **10000 + (n − 1)** (`0x2710` for R1), value `0xFF00` / `0x0000`. Same coil numbers as the read map (table coils 10000…10255) |
| Dual ACK | `mesh_ok` = request reached the node; `device_ok` = CRC-valid FC05 echo; else `error: TIMEOUT` |
| RAW-001 / TS-001 | No `CONTROL_RELAY`. OPEN is `ACCESS_OPEN` (prompt 17), not this op |
| Forbidden | `DISABLE_FIRE_ALARM`, `DISARM_ALL`, `ARM_ALL`, `SILENCE`, `SILENCE_ALARM`, `FIRE_RESET`, `RESET_ALARM`, `DISABLE_ALARM`, `FIRE_DISABLE`, `DISARM`, `ARM` |

Empty A/B with R1 allowed: TX happens, no reply → `TIMEOUT`. That is the honest bench result, not a fake device ACK.

## Preferred hardware path

```
Orion devices  ↔  С2000-ПП (Modbus RTU, official)  ↔  isolated RS-485  ↔  Heltec
```

С2000-ПП manuals document Orion ↔ Modbus, zone/relay/section counts (**512** zones, **255** relays, **64** partitions in the converter DB), 9600 8N1 on the converter’s host side as specified in the current PDF. Map those registers into Device / Point / Event. Frozen read map: `docs/reference/c2000pp-modbus.md`. Prompt 10 path: `docs/reference/orion-path-decision.md`.

[hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) (Apache-2.0) polls **HUB-C2000PP** (TCP 22000 PING/PONG, UDP push on +1). Use it for **state naming and push-vs-poll**, not as an RS-485 stack. Entity names: `docs/reference/hubc2000pp-entities.md`. Clone: `vendor/hass-hubc2000pp`. Optional later: a **server** connector to HUB-C2000PP for sites that already run that software — still not inside the ESP32 UART driver.

## Native Orion path (Signal-10 / Signal-20 MONITOR)

Typical bus: RS-485 «Орион», **9600**, 1 stop, no parity.

Frame layout + CRC: `docs/reference/orion-frames.md`. Signal-10 adapter probe: `docs/reference/signal10-orion.md`. Signal-20 adapter probe: `docs/reference/signal20-orion.md` (same Habr status-read; no S20-specific dump). CRC is **one byte at the end**. `crc8_orion` is used **only** by `bolid/signal10` and `bolid/signal20` (not `RS485_TRANSACTION`).

**No Signal-10 / Signal-20 CONTROL:** dumps justify status-read, not a named relay. Write-key `0x11` is not a MESH 485 capability. CXEM Signal-20 relay examples remain a **caveat**: CRC OK ≠ panel accepted (relay **program**, section, permissions).

Timings: `docs/reference/orion-timings.md` — design bounds (~50 ms DE, ~800 ms event, ~900 ms command, ~600 ms search), not gospel. Mesh round-trip must fit these or the adapter must **not** pretend to be a transparent modem. **`RAW_BRIDGE` is not the Orion path.**

## CXEM

Topic: [Протокол Орион (Болид)](https://forum.cxem.net/topic/105126-%D0%BF%D1%80%D0%BE%D1%82%D0%BE%D0%BA%D0%BE%D0%BB-%D0%BE%D1%80%D0%B8%D0%BE%D0%BD-%D0%B1%D0%BE%D0%BB%D0%B8%D0%B4/).

| Material | Use |
|---|---|
| Command spreadsheets, real hex dumps | unit tests / CRC check (`docs/reference/orion-vectors/`) |
| Signal-20 relay examples | capability + config caveats |
| Addressing discussion | address tests |
| Decrypted firmware | **do not** vendor into this repo |

A correct CRC still does not mean the panel accepted the command (relay program, section state, permissions).

## С2000-М

С2000-М is usually the **Orion RS-485 master**. Heltec must not be a second master. Wiring, listen-only tap vs Path A, why **not** RS-232 on this node, and why events are **not** MeshCore private chat: [`c2000m.md`](c2000m.md). Firmware for listen-only → EventStore is queued as [`../agent/prompts/24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md) (after persist protocol 23, **done**). Native Orion CONTROL remains leftover.

## Adapter order

1. С2000-ПП Modbus READ_ONLY (prompt 10) — **done** (this bench: empty bus, Path A stub that times out).  
2. CONTROL allow-list (14) — **done** (named `CONTROL_RELAY` only; default allow-list empty).  
3. Signal-10 (15) — **done** (MONITOR status-read; no CONTROL; manual CONFIG protocol).  
4. Signal-20 (16) — **done** (MONITOR status-read; no CONTROL; relay-program caveat in docs).  
5. С2000-М listen-only MONITOR → MESH 485 events — **queued** (prompt 24). Not MeshCore DM. Not second master.

## Why commands fail

Mesh → RS-485 TX → CRC OK → right address → command allowed → panel state → relay config → execute. Surface each step in diagnostics.
