# Signal-10 Orion MONITOR (prompt 15)

Freeze for `bolid/signal10`. This is **MONITOR / read**, not CONTROL.

Official product (Сигнал-10 РЭ): **10** ШС, RS-485 «Орион», **9600 8N1**, address **1..127**. Frame layout + CRC: `docs/reference/orion-frames.md`.

## Probe (dump-justified)

Habr 563584 (cites CXEM topic 105126) encrypted **status request** (cmd plaintext `0x57` / arg `0x02`):

```
83 08 00 ED B8 BA BA BA 62
```

Address 3, `MESSAGE_KEY = 0xBA`. Adapter default address is **1**; same layout, CRC-8/MAXIM recomputed. `crc8_orion` is wired **only** here — not to `RS485_TRANSACTION`.

Empty bus / flipped CRC → `S10-001` `online: false`. No invented FIRE / zone ALARM.

CRC-valid reply → `online: true`, diagnostic point `S10`, zone `Z1` state `UNKNOWN` (dumps have status bytes, not a 10-zone bitmap). Event: optional `DIAG` with `device_ok: true`.

## CONTROL

Prompt 14 capabilities exist (`CONTROL_RELAY` on PP-001 when allowed). **No Signal-10 CONTROL this run:** dumps justify status-read and write-key `0x11` (not a product capability). No relay opcode for Сигнал-10 in `docs/reference/orion-vectors/`. `execute()` stays `NO_PERMISSION`. `CONTROL_RELAY` is not advertised.

## CONFIG

Manual `CONFIG_SET` `protocol` in RAM: `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10`. **`AUTO` is refused.** TX on UART1: С2000-ПП probe unless protocol is `BOLID_SIGNAL10` (then this adapter). No NVS. No bus scan.
