# Remote RS-485 bridge

Windows/Android can open a **service port** through MeshCore:

```
PC  ↔  MESH 485 BRIDGE_OPEN  ↔  session  ↔  Rs485Hal (UART1)  ↔  A/B
```

This is **not** a transparent UART tunnel and **not** MeshCore `WITH_RS232_BRIDGE` (that carries MeshCore packets on RS-232). Traffic stays MESH 485 envelopes (whole frames + timeout), same as `RS485_TRANSACTION`. Never `UART byte → one LoRa/Companion packet`.

USB mux: first byte `{` is NDJSON MESH 485; everything else is Companion. Do not dump raw UART onto CDC. BLE NUS is not muxed this phase.

## Levels (`BRIDGE_OPEN` payload `level`)

| Level | Allowed | Default |
|---|---|---|
| `READ_ONLY` | Capture RX into a RAM ring (8×256). **No** `writeFrame` / **no** `RS485_TRANSACTION` while the session is open. | **yes** (omit `level` or send `READ_ONLY`) |
| `CONTROL` | Refused (`NO_PERMISSION`) unless `-D MESH485_ALLOW_BRIDGE_CONTROL=1`. Session may open. `COMMAND_EXECUTE` is a **separate** path (prompt 14): capability-gated, not implied by this session. No fire opcodes. | no |
| `RAW_BRIDGE` | Lab only. Must be **explicit** (`"level":"RAW_BRIDGE"`). Then `RS485_TRANSACTION` is allowed (existing codec/HAL). Operator + ts + level stored in a RAM last-session line (not OLED). | no |

Unknown `level` → `UNKNOWN_CMD`.

`READ_ONLY` must not accept arbitrary TX onto Orion (or any bus). That is the fire/safety line.

`RAW_BRIDGE` is unsuitable as the permanent Orion path (turnaround ~50 ms / command ~800–900 ms). Prefer `RS485_TRANSACTION` even in lab; a RAW session means opaque frames with timeout, not byte streaming.

## Session

- **Authz:** `BRIDGE_*` still treated as ADMIN once the envelope name resolves. Unknown / upgraded `operator` → `BAD_AUTH` (phase 20). Roles VIEWER / OPERATOR / ADMIN apply to `COMMAND_EXECUTE`. Replay window from Phase 06 still applies (`id`+`operator` 30 s, stale `ts` 120 s when unix time is known).
- **One session per port** (MVP = one isolated UART). Second `BRIDGE_OPEN` → `BUSY`.
- Idle timeout: default **30 s** (payload `idle_s`, cap 300) with no `BRIDGE_*` / (if RAW) `RS485_TRANSACTION` → auto close, DE RX. Session is RAM only; reboot → MONITOR.
- Close on `BRIDGE_CLOSE` (idempotent if none open), idle timeout, or USER long-press abort. Always `setDirection(false)` / DE low.
- Concurrent `RS485_TRANSACTION` during `READ_ONLY` (or `CONTROL`) → `NO_PERMISSION` (not a silent TX).
- Without a session (MONITOR): Phase 06 behaviour — `RS485_TRANSACTION` still works as a local diagnostic.
- `CONFIG_GET` `mode`: `MONITOR` vs `BRIDGE`. `NODE_STATUS` `session`: `"off"` or `{ "level":"READ_ONLY", "idle_s": <remaining> }`.

## RX path (`READ_ONLY`)

Idle `rs485.read()` feeds OLED only when no session. During an open session, chunks from `read()` are queued (drop oldest if the ring is full). No echo onto the bus.

Host drains with **`BRIDGE_POLL`** (alias `BRIDGE_DRAIN`):

```json
{ "session": true, "level": "READ_ONLY", "frames": [ { "ts": 0, "frame": "<base64>" } ], "dropped": 0 }
```

Empty ring is `OK` with `frames: []`. Drain is request/response only — no unsolicited NDJSON on USB CDC (Companion clients share the port). POLL may return fewer frames than the ring holds if they would not fit one MESH 485 response; leftovers stay queued. Chunks are whatever `read()` returned (not coalesced into gap-framed packets).

## DE fail-safe

GPIO 26 is HIGH only while `writeFrame` is transmitting. Open, close, timeout, abort, and the end of a RAW transaction all leave DE **RX** (low). Loss of MeshCore / host idle must not assert DE TX.

## OLED

While a session is open: `BRIDGE` + level (`READ_ONLY` / `RAW_BRIDGE` / `CONTROL`), `USER=abort`. READY line shows `BR RO` / `BR RAW` / `BR CTL`. USER **long-press** aborts → MONITOR / READY. Emergency still wins. The panel **stays awake** while the session is open (all levels); abort or timeout returns to READY (which can DISPLAYOFF after 10 s). Wake on open and on abort.

## USB mux (COM23)

```
cd firmware/heltec_v4
python scripts/m485_bridge.py COM23 open
python scripts/m485_bridge.py COM23 poll
python scripts/m485_bridge.py COM23 txn    # NO_PERMISSION while READ_ONLY
python scripts/m485_bridge.py COM23 close
```

Golden examples: `protocol/mesh485/examples/bridge_*.json`.
