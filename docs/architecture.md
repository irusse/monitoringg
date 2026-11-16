# Architecture

## Formula

```
MESH 485 Extender
  = MeshCore
  + isolated RS-485 HAL
  + Device Adapter framework
  + Bolid adapters (C2000 / Signal-10 / Signal-20)
  + Modbus RTU + RAW
  + PROFILE_ACCESS (Wiegand turnstile)
  + MESH 485 Protocol
  + Android/Windows console
  + optional server
```

Heltec V4 is a **universal wireless RS-485 port**, not a Bolid-only box and not a MeshCore chat node. С2000-М wiring and why fire events are MESH 485 events (not Companion DM): `docs/c2000m.md`.

```
Android / Windows / Server
            │
     MESH 485 API  (Companion BLE/USB/Wi-Fi + LoRa)
            │
        MeshCore
            │
        Heltec V4
       /    |    \
  LoRa   RS-485   Wiegand/relay
            │
     fire / security / meters / PLC / ACS
```

## Layering (do not skip)

```
MeshCore
   ↓
MESH 485 Protocol     ← commands, events, bridge sessions
   ↓
Device Abstraction    ← Device, Point, Event
   ↓
Adapter               ← bolid/c2000, signal10, signal20, modbus, access
   ↓
RS-485 HAL / GPIO HAL
```

Adding Rubezh, Argus, meters, or a PLC is a new adapter + registry entry. MeshCore, Flutter shell, and the event store stay unchanged.

## Node modes

| Mode | Job | Default |
|---|---|---|
| MONITOR | poll / listen → events on mesh | **yes** |
| BRIDGE | remote RS-485 service port | off; needs auth; READ_ONLY / CONTROL / RAW_BRIDGE |
| CONTROLLER | operator console executes **declared** capabilities | off |

## Profiles on one image

| Profile | I/O | Offline if server/mesh down |
|---|---|---|
| `PROFILE_RS485` | isolated UART + DE/RE | local queue + OLED |
| `PROFILE_ACCESS` | Wiegand 15/16, OPEN 40, lamps, sensors | local ACL (phase 17) |
| `PROFILE_REPEATER` | MeshCore forward flag | radio only |

Two physical RS-485 ports are a later hardware revision. MVP: **one isolated port**. GNSS header UART (GPIO 38/39) is the candidate for that port so Access pins stay put.

## Bolid: two legal paths

**Path A (preferred for a working fire integration):** bus master is Bolid. Heltec talks **Modbus RTU** to [С2000-ПП](https://bolid.ru) (official Orion ↔ Modbus converter). Zones / relays / sections map like hass-hubc2000pp.

**Path B (native Orion):** Heltec is a protocol-aware peer on Orion RS-485 (9600 8N1 typical). Frames, CRC-8, and timeouts must be verified against official docs and real dumps. MONITOR first. CONTROL only through capabilities.

`hass-hubc2000pp` is Apache-2.0 and useful for **entity/event mapping**. It speaks TCP/UDP to **HUB-C2000PP**, not Orion UART. Do not copy it into firmware as a parser.

## Mesh vs UART

Do not map `UART byte → LoRa packet`. Bolid radio tests use ~50 ms turnaround and ~800–900 ms command timeouts. Carry a **transaction**:

```
RS-485 request frame → MESH 485 request → remote or local adapter → RS-485 → response → MESH 485 response
```

Transparent RAW bridge is lab/diagnostics and Modbus-tolerant gear only.

## Dual ACK

1. **Mesh ACK** — packet reached the node.  
2. **Device ACK** — instrument answered (and, separately, accepted the command).

CRC proves the frame was intact, not that the panel executed the function.

## Operator app

One Flutter app (`app/mesh485`): Android = Windows.

Allowed: Dashboard, Objects, Devices, Events, Control, Bridge, Diagnostics, Settings, Access (turnstiles/employees), **extender node list** (RSSI/SNR/uptime of MESH 485 nodes).

Forbidden: MeshCore chat, contacts, channels, QR, photos on radio.

Radio identity (keys, repeater CLI) remains MeshCore tools when needed; the console shows **extender health**, not a messenger.

## Server

Optional. Node must monitor and queue events with no cloud.

## Upstream

| Need | Take | Do not take |
|---|---|---|
| LoRa / BLE+USB companion / repeater | MeshCore | chat UI on OLED or in the console |
| Bolid zones/relays/sections/push mapping | hass-hubc2000pp concepts | HA custom component, HUB-C2000PP as a required runtime |
| Orion ↔ Modbus registers | official С2000-ПП manuals | guessing opcodes |
| Orion dumps / CRC tables | CXEM topic as **test vectors** | decrypted firmware as source |
| USB/BLE/TCP ideas | meshcomod as notes | fork MeshCore |
| Wiegand / door pulse | esp32-rfid + WiegandNG | their GPIO map |
| ACS screens / DB fields | PiDoors UI + schema | PHP, MariaDB, Pi GPIO |

## Safety

MeshCore off → fire panel still runs on its own RS-485. This product must not be marketed as a certified fire control panel replacement.
