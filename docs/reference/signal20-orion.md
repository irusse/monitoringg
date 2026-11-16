# Signal-20 Orion MONITOR (prompt 16)

Freeze for `bolid/signal20`. This is **MONITOR / read**, not CONTROL. Not a second С2000-ПП. Prompt 10 `PP-001` stays Modbus.

Official product (Сигнал-20 / Сигнал-20П РЭ): **~20** ШС, RS-485 «Орион», **9600 8N1**, address **1..127**. Relay count is a product fact; **no opcode** is taken from the РЭ. Frame layout + CRC: `docs/reference/orion-frames.md`.

## Probe (dump-justified)

No Signal-20-specific event/zone read frame is published in `docs/reference/orion-vectors/`. MONITOR reuses the same family as Signal-10: Habr 563584 (cites CXEM topic 105126) encrypted **status request** (cmd plaintext `0x57` / arg `0x02`):

```
83 08 00 ED B8 BA BA BA 62
```

Address 3, `MESSAGE_KEY = 0xBA`. Adapter default address is **1**; same layout, CRC-8/MAXIM recomputed (`orion_build_status_request` shared with Signal-10). `crc8_orion` is wired **only** to these MONITOR adapters — not to `RS485_TRANSACTION`.

Empty bus / flipped CRC → `S20-001` `online: false`. No invented FIRE / zone ALARM. Diagnostic point `S20` type `loop` state `FAULT`. `EVENT_LIST` for this device stays `[]`.

CRC-valid reply → `online: true`, diagnostic point `S20`, zone `Z1` state `UNKNOWN` (dumps have status bytes, not a 20-zone bitmap). Event: optional `DIAG` with `device_ok: true`. Do **not** map 20 FIRE zones.

## CONTROL

Prompt 14 capabilities exist (`CONTROL_RELAY` on PP-001 when allowed). **No Signal-20 CONTROL this run:** `orion-vectors/` has no native Orion relay opcode. Write-key `0x11` is not a product capability. `execute()` stays `NO_PERMISSION`. `CONTROL_RELAY` is not advertised.

CXEM row “Signal-20 relay examples” (`docs/bolid.md`) is the **caveat**, not a TX table: a CRC-valid frame on the wire does **not** mean the relay clicked. Result depends on **relay program** in the panel (and section / permissions). If a later dump cites a named relay frame: `device_ok: true` = CRC-valid echo / accepted frame, **not** “contact closed”. Do not synthesize a click. Empty bus after an allowed TX → `TIMEOUT` (`mesh_ok: true`, `device_ok: false`).

## CONFIG

Manual `CONFIG_SET` `protocol` in RAM: `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10` | `BOLID_SIGNAL20`. **`AUTO` is refused.** TX on UART1: С2000-ПП probe unless protocol is `BOLID_SIGNAL10` (S10) or `BOLID_SIGNAL20` (this adapter). No NVS. No bus scan. Bridge `READ_ONLY` / `CONTROL` session: skip probe.
