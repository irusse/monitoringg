# Orion native frames (Path B)

Freeze date: 2026-08-28. This is a **parse/CRC spec**, not an adapter. Do not TX these frames on UART1 this run. Command bytes below are **codes seen in dumps** — not an execute table.

Official Bolid RS-485 «Орион» host parameters (С2000-ПП РЭп, table 1.1.2.4): **9600**, 1 start / 1 stop, **no parity**, max packet **75** bytes. Device address on the Orion line: **1..127** (same PDF, event field 3593).

The native UART framing is a closed Bolid protocol (no public «кадр Орион» PDF with a byte map). Layout below is frozen from published bus dumps that cite the CXEM thread, then CRC-checked in native tests. Anything not in those dumps or the С2000-ПП PDF is `UNCONFIRMED`.

## Byte map

| Field | Where | Notes |
|---|---|---|
| Address | byte 0 | `1..127` = plaintext peer. Encrypted frames use `address \| 0x80` (Habr 563584). Range 1..127 also in С2000-ПП event register 3593. |
| Length | byte 1 | **Count of bytes after the address, including this length byte and the CRC.** Equivalently `total_frame_len - 1`. Worked: 7-byte frame → length `0x06`; 9-byte → `0x08`; 11-byte → `0x0A`. |
| Command / code | starts at byte 2 | Plaintext: after an optional key-xor byte. Encrypted: command bytes are XOR’d with `MESSAGE_KEY`. **Do not implement execution.** Codes seen in the fixtures: `0x11` (write global key, plaintext); status read appears as `0xED`/`0xB8` which the Habr write-up decodes as `0x57^KEY` / `0x02^KEY`. |
| Payload | after command, before CRC | Key material, status bytes, padding. Encrypted payload is XOR’d; a correct CRC still does **not** mean the panel accepted the function (relay program, section, permissions). Dual ACK is prompt 11. |
| CRC | **last byte only** (one octet, not CRC-16) | CRC-8 over all bytes **except** the CRC itself. Residue over the full frame (including CRC) is `0x00` for the verified algo. |

```
[ ADDR ][ LEN ][ CMD / params … ][ CRC8 ]
   0       1     2 .. n-2           n-1
```

Encrypted vs plaintext is **not** a second CRC. Bit 7 of ADDR is only a dump-observed flag (`UNCONFIRMED` as a protocol rule beyond the three fixtures).

## CRC algorithm (matched)

**Production `crc8_orion` = CRC-8/MAXIM-DOW (Dallas 1-Wire).**

| Parameter | Value |
|---|---|
| Width | 8 |
| Polynomial | `0x31` (reflected on the wire as `0x8C`) |
| Init | `0x00` |
| RefIn / RefOut | true / true |
| XorOut | `0x00` |
| Check (`"123456789"`) | `0xA1` |
| Placement | last byte of the frame |

This is the table starting `00 5E BC E2 …` published next to the dumps. Native tests compare:

| Algo | Result on fixtures |
|---|---|
| `Crc8Algo::Orion` (`crc8_orion`) | **match** |
| `Crc8Algo::DallasMaxim` | **match** (same poly/init as Orion) |
| `Crc8Algo::Generic` (poly `0x07`, not reflected) | **mismatch** |

A future dump that fails `crc8_orion` should fail the test loudly; do not silently switch polynomials.

## Worked examples

CRC is computed on the prefix (everything except the last byte). Expected CRC is the last byte.

### 1. Plaintext — set global key (address 3)

```
03 06 00 11 BA BA 8D
```

| Byte | Value | Role |
|---|---|---|
| 0 | `03` | address 3 (plaintext, bit7 clear) |
| 1 | `06` | length = 7 − 1 |
| 2 | `00` | `GLOBAL_KEY ^ MESSAGE_KEY` (same keys → 0) |
| 3 | `11` | command **0x11** (write key) — document only |
| 4–5 | `BA BA` | new key |
| 6 | `8D` | CRC-8 |

`crc8_orion(03 06 00 11 BA BA) = 8D` → **pass**. Flipping bit 0 of ADDR → CRC ≠ `8D` → **fail**.

Source: [Habr «Дешифрация протокола Орион Bolid»](https://habr.com/ru/articles/563584/) (article 563584; cites [CXEM topic 105126](https://forum.cxem.net/topic/105126-%D0%BF%D1%80%D0%BE%D1%82%D0%BE%D0%BA%D0%BE%D0%BB-%D0%BE%D1%80%D0%B8%D0%BE%D0%BD-%D0%B1%D0%BE%D0%BB%D0%B8%D0%B4/), started 2012-03-21). Captured into this repo 2026-08-28. CXEM posts themselves are JS-gated; these published hex strings are the legal fixtures.

### 2. Encrypted — status request (address 3)

```
83 08 00 ED B8 BA BA BA 62
```

| Byte | Value | Role |
|---|---|---|
| 0 | `83` | `0x03 \| 0x80` |
| 1 | `08` | length = 9 − 1 |
| 2 | `00` | key xor |
| 3–4 | `ED B8` | command bytes as seen (status read, XOR’d) |
| 5–7 | `BA BA BA` | message key / padding as published |
| 8 | `62` | CRC-8 |

`crc8_orion(83 08 00 ED B8 BA BA BA) = 62` → **pass**. Same source as example 1.

### 3. Encrypted — status reply (same session)

```
83 0A E2 B8 BA BE B9 7D 2F 72 D7
```

Length `0x0A` = 11 − 1. Last byte `D7` is CRC. `crc8_orion` over the first 10 bytes = `D7` → **pass**. Same source.

The Habr decode of status bytes (`0xC7` / `0x95` after XOR) is **not** required to validate CRC. CRC = frame intact, not “panel accepted”.

## Inter-byte gap vs HAL

Orion character-gap in an official protocol PDF: **`UNCONFIRMED`** (native spec is closed). С2000-ПП Modbus side documents end-of-frame as a pause **≥ 3.5 character times** (РЭп §1.1.4.3 / figure 2) — that is Modbus, not a proven Orion UART rule.

Current `Rs485Hal::readFrame` already stops on an inter-byte gap after the first RX byte, else `timeout_ms`. At 9600 the gap is `max(5 ms, 3.5 char-times)` (clamped 5..50 ms). **Keep that HAL.** Do not add a 2 s wait. Recommended for prompt 10 Path B listen: same gap; do not tighten below 5 ms until a live capture says otherwise.

## Out of scope

- No Signal-10 / Signal-20 CONTROL opcodes on A/B (write-key `0x11` is documented only).
- `RS485_TRANSACTION` stays opaque RAW (dispatcher `error: CRC` unused).
- `DEVICE_DISCOVER` includes `S10-001` and `S20-001`; AUTO scan is still later.
