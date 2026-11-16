# PROMPT 07 — Remote RS-485 bridge (session + READ_ONLY default)

You are implementing **MESH 485 Extender** in this repo. This is **run 7 of 19**. Do **only** this run. Stop when `BRIDGE_OPEN` / `BRIDGE_CLOSE` work (native tests + USB mux) with default **READ_ONLY**, one session per port, idle timeout, DE fail-safe RX, and OLED `BRIDGE` — **not** UART-byte-per-LoRa-packet, **not** MeshCore `WITH_RS232_BRIDGE`.

Phases **01–06 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1, MESH 485 JSON dispatcher + `RS485_TRANSACTION` + USB mux).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile.

```
MeshCore → MESH 485 Protocol → Bridge session (this run) → Rs485Hal
         → (DeviceAdapter later, prompt 08)
```

This run is a **service-port session** on the existing HAL. No Bolid parser, no DeviceAdapter registry, no Flutter, no ACL, no `COMMAND_EXECUTE`, no `CONFIG_SET` persist, no AUTO discovery, no LoRa CBOR.

## Read first

- `AGENTS.md`
- `docs/architecture.md` (modes MONITOR / BRIDGE / CONTROLLER; never UART-byte → LoRa; dual ACK)
- `docs/bridge.md` (canonical levels — **keep this file true**)
- `docs/mesh485-protocol.md` (envelope; `BRIDGE_OPEN` / `BRIDGE_CLOSE`)
- `docs/security.md` + `SECURITY.md` (default READ_ONLY; RAW_BRIDGE lab-only; fire panel independent)
- `docs/oled-ui.md` + `docs/pinout.md` + `hardware/rs485-adapter.md`
- `firmware/heltec_v4/include/mesh485/dispatcher.h` (Phase 06 currently **refuses** `BRIDGE_*` with `NO_PERMISSION`)
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` + `usb_mux.cpp`
- `firmware/heltec_v4/include/hal/rs485.h` (UART1 TX39 / RX38 / DE26, 9600 8N1; DE high only while TX)
- `firmware/heltec_v4/src/main.cpp` (idle RX → OLED `RS485 N B`, **no echo**)
- `firmware/heltec_v4/include/display/access_display.h`
- `protocol/mesh485/README.md` (USB mux: first byte `{`)
- `vendor/MeshCore/src/helpers/bridges/RS232Bridge.*` — **do not enable**; that is MeshCore packets over RS-232, not MESH 485

## Rules

- GPIO **19/20 = USB PHY**. Do not move RS-485 there. Do not change the Access GPIO map.
- Do not fork `vendor/MeshCore`. Do not compile `ui-new` / `DISPLAY_CLASS`. Do **not** define `WITH_RS232_BRIDGE` / `WITH_ESPNOW_BRIDGE`.
- Keep Access OLED as home (READY / USER menu / 10 s DISPLAYOFF). While a session is open, show **`BRIDGE` + level**, not MeshCore chat. Do not show BLE PIN.
- **Never** `UART byte → one LoRa/Companion packet`. Bridge traffic is MESH 485 envelopes (whole frames + timeout), same as Phase 06.
- Photos never in MESH 485.
- Fire CONTROL / Bolid opcodes / `COMMAND_EXECUTE`: out of scope.
- Node must keep working if the host disconnects: idle timeout **must** close the session and leave DE **RX** (low). Loss of MeshCore must not assert DE TX.
- USB CDC after `mesh.start()` is Companion. Reuse the Phase 06 **USB mux** (`{` → dispatcher). Do not dump raw UART onto CDC. BLE NUS: do not mux JSON this run.

## Semantics (must match)

Windows/Android open a **service port**:

```
host  ↔  MESH 485 BRIDGE_OPEN  ↔  session  ↔  Rs485Hal  ↔  A/B
```

This is **not** a transparent UART tunnel. Protocol-aware by default.

### Levels (`BRIDGE_OPEN` payload `level`)

| Level | This run | Default |
|---|---|---|
| `READ_ONLY` | Capture RX into a RAM ring. **No** `writeFrame` / **no** `RS485_TRANSACTION` while the session is open. | **yes** (omit `level` or send `READ_ONLY`) |
| `CONTROL` | **Refuse** (`NO_PERMISSION`) unless `-D MESH485_ALLOW_BRIDGE_CONTROL=1`. Even with the flag: session may open, but `COMMAND_EXECUTE` stays refused (prompt 08/14). Do not TX fire opcodes. | no |
| `RAW_BRIDGE` | Lab only. Must be **explicit** in JSON (`"level":"RAW_BRIDGE"`). Then `RS485_TRANSACTION` is allowed (existing codec/HAL). Log operator + ts + level in a small RAM “last session” line (not OLED secrets). | no |

`READ_ONLY` **must not** accept arbitrary TX onto Orion (or any bus). That is the fire/safety line for this run.

`RAW_BRIDGE` is unsuitable as the permanent Orion path (turnaround ~50 ms / command ~800–900 ms). Prefer `RS485_TRANSACTION` even in lab; RAW session is “I know this is opaque frames with timeout,” not byte streaming.

Unknown `level` → `UNKNOWN_CMD` or `NO_PERMISSION` (pick one, document it).

### Session

- Authz: treat as ADMIN. **Stub is OK**: do not implement real roles (prompt 14/19). Document “authz stub” in `docs/bridge.md` / Phase 07 note. Replay window from Phase 06 still applies.
- **One session per port** (MVP = one isolated UART). Second `BRIDGE_OPEN` → `BUSY`.
- Idle timeout: e.g. **30 s** with no `BRIDGE_*` / (if RAW) `RS485_TRANSACTION` → auto `CLOSE`, DE RX. Cap any wait; do not starve `mesh.loop()`.
- Close on `BRIDGE_CLOSE`, timeout, or dispatcher `BUSY` teardown. Always `setDirection(false)` / DE low.
- Concurrent `RS485_TRANSACTION` during `READ_ONLY` session → `NO_PERMISSION` (not a silent TX).
- Without a session (MONITOR): keep Phase 06 behaviour — `RS485_TRANSACTION` still works as a local diagnostic.
- `CONFIG_GET` `mode`: `MONITOR` vs `BRIDGE`. `NODE_STATUS` should show session: `off` or `{ "level":"READ_ONLY", "idle_s":… }`.

### RX path (READ_ONLY)

Idle `main.cpp` `rs485.read()` currently feeds OLED only. During an open session, **queue** captured bytes/frames into a ring (e.g. 8 slots × 256 bytes, drop oldest). Do not echo onto the bus.

Host drains with a small extra op (name can vary, keep it one op):

- `BRIDGE_POLL` **or** `BRIDGE_DRAIN` — returns `{ "session": true, "level":"READ_ONLY", "frames":[ {"ts":…, "frame":"<base64>"} ], "dropped": N }`

Do **not** emit unsolicited NDJSON on USB CDC (Companion clients share the port). Drain is request/response only.

RAW = same gap/timeout rules as Phase 06 `readFrame` if you coalesce UART into frames; documenting “chunk whatever `read()` returned” is OK if the ring stores those chunks.

## Deliverables

### 1. Spec freeze (docs + examples)

Keep `docs/bridge.md` as source of truth; expand it to match the implementation (levels, timeout, one session, DE, OLED abort, authz stub, USB mux). Update `docs/mesh485-protocol.md` `BRIDGE_*` rows if JSON differs.

Add golden examples:

- `protocol/mesh485/examples/bridge_open.request.json`
- `protocol/mesh485/examples/bridge_open.response.json`
- `protocol/mesh485/examples/bridge_close.request.json`
- `protocol/mesh485/examples/bridge_poll.response.json` (or drain)

Envelope must stay Phase 06 shape (`v`, `type`, `id`, `node` `EXT-001`, `op`, `operator`, `ts`, `nonce`, `payload`).

`BRIDGE_OPEN` request payload example:

```json
{
  "level": "READ_ONLY",
  "idle_s": 30
}
```

Response payload example:

```json
{
  "session": true,
  "level": "READ_ONLY",
  "port": "RS485",
  "idle_s": 30
}
```

Errors (reuse Phase 06): `REPLAY`, `BAD_AUTH` (still stub), `UNKNOWN_CMD`, `BUSY`, `TIMEOUT`, `HARDWARE`, `NO_PERMISSION`.

### 2. Firmware (`src/mesh485/` + OLED)

Suggested split (names can vary, keep it small):

| File | Job |
|---|---|
| `bridge.*` (or session inside dispatcher) | open/close/poll, idle timer, ring, DE fail-safe |
| `dispatcher.*` | wire `BRIDGE_OPEN` / `BRIDGE_CLOSE` / drain; change Phase 06 refuse list |

Do **not** use `mesh_access/map_protocol.cpp` strstr.

`Rs485Hal` stays the only UART. Do not open a second serial for the bridge.

OLED: add `AccessDisplay::showBridge(const char* level)` (or equivalent). Layout ~128×64:

```
EXT/TS-001        M●
BRIDGE
READ_ONLY
USER=abort
```

USER **long-press** while this screen is up **aborts** the session → MONITOR / READY (document in `docs/oled-ui.md`). Emergency still wins. 10 s DISPLAYOFF: bridge screen should **wake** on open and on abort; do not blank away from an active RAW session without a written policy (prefer: stay awake while session open, or wake on POLL — pick one, document it).

READY line may show `BR RO` / `BR RAW` instead of `M485 9600` during a session.

### 3. Ops to implement (only these new)

| op | Behavior |
|---|---|
| `BRIDGE_OPEN` | Start session; default `READ_ONLY`; see levels above |
| `BRIDGE_CLOSE` | End session; DE RX; `session: false`. Close when none already → `OK` (idempotent) |
| `BRIDGE_POLL` or `BRIDGE_DRAIN` | Drain RX ring; empty ring is `OK` with `frames: []` |

Keep working from Phase 06: `NODE_STATUS`, `CONFIG_GET`, `DIAGNOSTICS`, `RS485_TRANSACTION` (gated by session as specified).

**Still refuse:** `COMMAND_EXECUTE`, `DEVICE_*`, `POINT_*`, `EVENT_*`, `CONFIG_SET`, `ACCESS_*`.

### 4. Cooperative loop

Do not block `mesh.loop()` / Wiegand / turnstile for seconds. Ring fill is non-blocking (`read()` in `loop()`). `RS485_TRANSACTION` cap stays ≤ 2000 ms (Phase 06).

### 5. Tests

Extend `[env:native_protocol_tests]` (fake in-memory port, **no** Arduino HAL):

- `BRIDGE_OPEN` omit level → `READ_ONLY`, `session: true`
- `RS485_TRANSACTION` during READ_ONLY → `NO_PERMISSION`
- Fake RX bytes → `BRIDGE_POLL`/`DRAIN` returns them base64
- Second `BRIDGE_OPEN` → `BUSY`
- `BRIDGE_OPEN` `CONTROL` → `NO_PERMISSION` (unless the test env defines the compile flag — then still no `COMMAND_EXECUTE`)
- `BRIDGE_OPEN` `RAW_BRIDGE` → `RS485_TRANSACTION` loopback still round-trips
- `BRIDGE_CLOSE` then `RS485_TRANSACTION` allowed again (MONITOR diagnostic)
- Idle timeout: advance fake clock → session closed, DE/RX equivalent (`ok` to TX again)

`pio run -e native_protocol_tests` must succeed (Windows: existing MSVC `g++` shim is OK).

### 6. On-device proof

- Build `heltec_v4_access` (`pio run -e heltec_v4_access`). Flash **COM23** when the port is free (Phase 06 hit PermissionError if a monitor held CDC; close it first).
- Boot: OLED READY, USER menu, 10 s blank still work; Access Wiegand path unchanged.
- USB mux: `NODE_STATUS` still works. Then `BRIDGE_OPEN` READ_ONLY → JSON `session: true`; OLED `BRIDGE`. Long-press USER → session ends, READY.
- `RS485_TRANSACTION` while READ_ONLY session open → `NO_PERMISSION`.
- Empty bus: do not fake instrument ACKs. `BRIDGE_POLL` empty is success. If no isolator, write that in `docs/implementation.md`.
- Companion: `{` lines still muxed; non-`{` still Companion. Optional: meshcore-cli `device query` if installed.

### 7. Docs

- `docs/implementation.md` Phase 07.
- `docs/bridge.md` + `docs/oled-ui.md` + `firmware/heltec_v4/README.md` (how to OPEN/POLL/CLOSE on the mux; abort; DE).
- `protocol/mesh485/README.md` — one paragraph on bridge vs `RS485_TRANSACTION`.

## Forbidden this run

- Bolid / Orion / Modbus exception parsing; DeviceAdapter registry (prompt 08).
- Flutter, FastAPI, photos, OTA, Excel.
- MeshCore chat UI, contacts on OLED.
- Editing `vendor/`. Enabling MeshCore RS232/ESP-NOW bridge.
- `COMMAND_EXECUTE` / fire silence / CONTROL as a real capability (prompt 14).
- Byte streaming RAW (one Companion/LoRa packet per UART byte).
- Changing Access GPIO map. Persisting bridge-always-on in NVS (session is RAM; reboot → MONITOR).

## Done when

1. Golden examples exist; native tests pass for the cases above.
2. `BRIDGE_OPEN` default READ_ONLY cannot TX; `RAW_BRIDGE` is explicit; `CONTROL` refused (or flag-gated as specified).
3. One session; idle timeout closes; DE not left in TX.
4. OLED shows `BRIDGE` + level; USER long-press aborts.
5. USB mux + `heltec_v4_access` still build; Phase 06 ops still work.
6. Phase 07 note in `docs/implementation.md`.

Then **stop**. Next cascade file: `agent/prompts/08-device-adapters.md`.
