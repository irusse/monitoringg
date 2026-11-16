# Path A vs Path B — decision for prompt 10

**Date:** 2026-08-28  
**Bench:** Heltec V4 on USB-C **COM23**, isolated RS-485 UART1 (TX39 / RX38 / DE26), USB-RS485 dongle available for A/B loopback. **No С2000-ПП**, **no C2000 / Signal-20 / Signal-10** on the isolated bus this freeze. Empty A/B still must not invent Bolid events.

## Decision

| If | Prompt 10 does |
|---|---|
| С2000-ПП (or equivalent official converter) on the isolated bus | **Path A**: `bolid/c2000pp_modbus`, read-only register poll, map to Points |
| No converter, but one native panel (C2000 **or** Signal-20) | **Path B**: one MONITOR adapter, listen/poll events only, CRC from this freeze |
| Neither (empty A/B, dongle only) | **This bench.** Product default remains **Path A**. Prompt 10 ships a **Modbus RTU read stub** (`bolid/c2000pp_modbus`) that issues a documented FC03/FC04 read, waits the existing transaction timeout, and returns **`device_ok: false`** / `TIMEOUT` on an empty bus. **No native MONITOR that TX.** **No write coils.** |

**Pick: Path A (product default) + empty-bus Modbus read stub.**

Why not Path B on this bench: there is no native panel to listen to; a MONITOR that never TX would still register a fake `BOLID_*` device with no events. Why not skip the adapter: prompt 10 is the first read-only Bolid adapter; a stub that times out cleanly keeps `DEVICE_DISCOVER` honest (`device_ok: false`) and reuses `docs/reference/c2000pp-modbus.md` when a converter appears.

`crc8_orion` is hooked to the Signal-10 MONITOR probe only (`docs/reference/signal10-orion.md`, prompt 15). `RS485_TRANSACTION` stays opaque RAW. Isolated PHY still required before any live converter is attached.

С2000-М as Orion **master**: Heltec must not TX as a second master. Path A (С2000-ПП) remains preferred. Listen-only tap and why events are not MeshCore private chat: `docs/c2000m.md`. Firmware queued as prompt 24 (protocol persist 23 is done).
