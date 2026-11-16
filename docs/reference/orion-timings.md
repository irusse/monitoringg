# Orion / RS-485 timings (design bounds)

Freeze date: 2026-08-28. Numbers below are **design bounds, not gospel**. They come from Bolid radio-repeater notes (С2000-РПИ) plus the conservative padding already in `docs/bolid.md`. A wired-only bus is faster; Mesh/Companion is slower. Do not treat any row as a certified fire-system timeout.

## Bounds used by this node

| Event | Bound (ms) | Implication for this node |
|---|---|---|
| Direction change / DE | ~50 | HAL already holds DE after TX (`holdDeAfterTx` ≈ 2 bit-times, then DE low). Do **not** add 2 s waits. Official С2000-РПИ “pause before session with direction change” is **30 ms** (RЭп table 2.2.3); ~50 ms is a padded design bound. |
| Event reply | ~800 | `RS485_TRANSACTION` default **900** ms (`MESH485_TXN_TIMEOUT_DEFAULT_MS`) is in-band. Official R=0 radio formula is `300 + 2*(R+1)*30` = **360 ms**; ~800 covers several radio hops. |
| Command | ~900 | Cap stays **≤ 2000** (`MESH485_TXN_TIMEOUT_MAX_MS`). Official R=0: `600 + 2*(R+1)*30` = **660 ms**. |
| Search / scan | ~600 | AUTO discovery is later; **do not scan this run**. Official R=0: `30 + 2*(R+1)*30` = **90 ms**. ~600 is a multi-hop pad. |

Source for the formulas: [С2000-РПИ РЭп](https://portal.bolid.ru/files/373/566/s2000_rpi_rep_sep_22.pdf) АЦДР.425664.001, Изм.4 от 06.09.2022, tables 2.2.2–2.2.4 (`R` = radio relay count; `R = 0` if unused). С2000-ПП Orion host: 9600 8N1, max packet 75 bytes (АЦДР.426469.020 РЭп, table 1.1.2.4). С2000-ПП Modbus “pause before answer on Orion” is a **2 ms** config item in older РЭ — that is converter-local, not a mesh bound.

## Why `RAW_BRIDGE` is not the Orion path

Mesh / Companion round-trip is **not** a 50 ms UART turnaround. Byte-streaming a fire/security bus through LoRa or USB Companion **desynchronizes** the panel: the master sees late edges, retries, and lost slaves.

Permanent path = **whole-frame** `RS485_TRANSACTION` (and, in prompt 10, adapter-owned transactions with a timeout in the table above). `READ_ONLY` bridge captures RX only — that stays the safety line. `RAW_BRIDGE` remains lab-only (`docs/bridge.md`): explicit level, opaque frames + timeout, never `UART byte → one LoRa/Companion packet`.

## HAL gap vs Orion inter-byte

`Rs485Hal::readFrame`:

- Wait up to `timeout_ms` (capped 2000) for the first byte.
- After the first byte, stop when the inter-byte gap elapses.
- Gap = `35000 / baud` ms (≈ 3.5 character times at 10-bit frames), **clamped 5..50 ms**. At default 9600 that is **5 ms**.

Orion official inter-byte: **`UNCONFIRMED`** (native protocol PDF not public). Modbus on the С2000-ПП host side uses ≥ 3.5 char-times as end-of-frame — keep the current HAL; do not invent a longer Orion-only gap this run.

DE: GPIO 26 HIGH only during `writeFrame`; idle is RX. Loss of mesh must not leave DE in TX (`docs/security.md`).
