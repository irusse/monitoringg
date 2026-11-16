# С2000-ПП Modbus — minimal read map (docs only)

**Manual:** Преобразователь протокола «С2000-ПП», руководство по эксплуатации **АЦДР.426469.020 РЭп**, Изм.17 от **21.08.2025**.  
PDF: https://bolid.ru/files/373/566/s2_pp_rep_sep_2025.pdf (bolid.ru, fetched 2026-08-28).

This is **read-first**. Writes are capability-gated named coils only (prompt 14). Confirm addresses against the PDF page (table **1.1.5.14.1** and §1.1.5) — do not guess new ranges.

## Host side (Heltec ↔ С2000-ПП Modbus port)

From table **1.1.2.5** (шлюз Modbus):

| Item | Spec |
|---|---|
| Protocol | Modbus-RTU, Heltec is **client**, converter is **slave** |
| Interface | RS-485 |
| Baud | 1200, 2400, **9600**, 19200, 38400, 57600, 115200 (configured on the converter) |
| Parity | none / even / odd |
| Stop | 1 or 2 without parity; **2** with parity |
| Max ADU | 256 bytes |
| End of frame | pause ≥ **3.5** character times (figure 2 / §1.1.4.3) |
| Slave address | 1..247 (unique on the Modbus side) |

Default bench UART remains **9600 8N1** until the converter’s configured host settings are known. Isolated A/B required on a live bus.

Orion side of the same box (table **1.1.2.4**, not the Heltec port): 9600 8N1, max packet **75** bytes. Bus master on Orion stays Bolid.

DB capacity (table **1.1.2.7**): **512** zones, **255** relays, **64** sections, event ring 256.

## Read function codes (table 1.1.5.6.1)

| FC | Name | Use in prompt 10 (read-only) |
|---|---|---|
| `0x01` | Read Coils | Relay state bits, coils **10000..10255** (реле №1..255) |
| `0x02` | Read Discrete Inputs | Optional: Orion master/slave, link, tamper (coils 8..10) |
| `0x03` | Read Holding Registers | Zone / section **state** (below) |
| `0x04` | Read Input Registers | Config tables + event ring + extended zone state |

Do **not** use `0x06` / `0x10` this run (holding/register presets). Exception codes include 15 = data not yet known — retry later, do not invent a Point state.

## Write coils — named `CONTROL_RELAY` only (prompt 14)

FC **0x05** Write Single Coil, same coil numbers as the read map:

| Coil DEC | HEX | Point | Action |
|---|---|---|---|
| 10000 | `0x2710` | `R1` | `ON` = `0xFF00`, `OFF` = `0x0000` |
| 10000 + (n − 1) | | `Rn` (n = 1…8) | same |

Firmware **does not TX** unless panel config allows that `n` (`allowRelay` / `MESH485_ALLOW_PP_RELAY`). Never a multi-coil / “all relays” / fire-disable write. Empty bus → `TIMEOUT`, `device_ok: false`.

## Holding registers — state (table 1.1.5.14.1)

| Range (DEC) | HEX start | Content |
|---|---|---|
| 40000 .. 40511 | `0x9C40` | Zone №1 .. №512 **state** (one register per zone). Addr = `40000 + (zone − 1)`. §1.1.5: FC **3**. |
| 44096 .. 44159 | `0xAC40` | Section №1 .. №64 **aggregate state**. Addr = `44096 + (section − 1)`. FC **3**. |
| 30000 .. 30511 | `0x7530` | Optional ADC per zone (Orion-Master analog). Not required for the stub. |
| 46152 / 46153 | `0xB448` / `0xB449` | Device type (**36** = С2000-ПП) and firmware version. Useful probe. |
| 46160 .. 46162 | | Newest / oldest / unread event counts. Read later (prompt 11). |

Extended zone state in **input** space: `4096 + (zone − 1) * 16` .. 16 registers per zone (4096..12287). FC **4**.

## Input registers — config (FC 4, addresses 0..2687)

| Formula | Content |
|---|---|
| `((zone − 1) * 4) + 0` | Zone table: Orion device address |
| `+ 1` | Zone number on that device (ШС) |
| `+ 2` | Modbus section id |
| `+ 3` | Zone type |
| `((relay − 1) * 2) + 2048` | Relay table: device address |
| `+ 1` | Relay / ШС number on device |
| `(section − 1) + 2560` | Section identifier (2560..2687 → sections 1..64) |

Event ring (oldest event): input **3584..3600**. Reading **3600** **deletes** the event from the buffer — treat as side-effecting; prompt 10 stub should **not** poll 3600.

## Coils — relay read (FC 1)

| Coil DEC | HEX | Content |
|---|---|---|
| 10000 | `0x2710` | Relay №1 |
| 10000 + (n − 1) | | Relay №n |
| 10255 | `0x280F` | Relay №255 |

## Probe coded in firmware (prompt 10)

`C2000ppModbusAdapter` (`PP-001`) issues **FC 3**, start **46152** (`0xB448`), quantity **2**, slave **1**, CRC-16 Modbus (low byte first), timeout **900** ms. Cites table **1.1.5.14.1** (type + version) and table **1.1.5.6.1** (FC 3). Empty bus → Device `online: false` (HAL `TIMEOUT` / no CRC-valid ADU). Does **not** synthesize type 36. Does **not** read input **3600**. Optional follow-up: zone №1 holding **40000** (same table) after a CRC-valid type **36** reply.

Map later: holding zone/section/coil values → `Point` (`zone` / `section` / `relay`) using names from `docs/reference/hubc2000pp-entities.md`.
