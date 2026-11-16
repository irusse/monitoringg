# PROMPT 06 — MESH 485 protocol (codec + dispatcher + RS-485 transaction)

You are implementing **MESH 485 Extender** in this repo. This is **run 6 of 19**. Do **only** this run. Stop when JSON MESH 485 request/response works (native tests + on-device dispatcher) and `RS485_TRANSACTION` talks to the existing `Rs485Hal` — **not** UART-byte-per-LoRa-packet.

Phases **01–05 are done** (pinout, BLE+USB companion, OLED Access UI + 10 s blank, Wiegand/turnstile HAL, isolated RS-485 UART1).

## Product (do not expand)

Heltec V4 = MeshCore **transport** + MESH 485 **application layer** + RS-485 HAL + Access profile.

```
MeshCore → MESH 485 Protocol → (adapters later) → Rs485Hal
```

This run is the **application envelope and a RAW transaction**. No Bolid parser, no DeviceAdapter registry, no Flutter, no ACL, no BRIDGE_OPEN (that is prompt 07), no CONFIG_SET persist, no AUTO discovery.

## Read first

- `AGENTS.md`
- `docs/architecture.md` (layering, dual ACK, never UART-byte → LoRa)
- `docs/mesh485-protocol.md` (canonical envelope and ops)
- `docs/protocol-3map.md` (Access subset — **do not implement** ACCESS_* this run; leave a comment/namespace hook)
- `docs/pinout.md` + `hardware/rs485-adapter.md` (UART1 TX39 / RX38 / DE26, 9600 8N1)
- `firmware/heltec_v4/include/hal/rs485.h`
- `firmware/heltec_v4/src/main.cpp` (Phase 05 **byte-echo** — replace)
- `firmware/heltec_v4/src/meshcore_adapter/meshcore_service.cpp` (USB CDC is Companion **after** `usb_serial_interface.begin(Serial)`)
- `vendor/MeshCore/docs/companion_protocol.md` (USB/BLE frames are **binary**; first byte is a command id, not `{`)
- Existing stub `src/mesh_access/map_protocol.*` (Access 3MAP toy parser — do not build MESH 485 on top of that strstr hack; new `src/mesh485/`)

## Rules

- GPIO **19/20 = USB PHY**. Do not move RS-485 there.
- Do not fork `vendor/MeshCore`. Do not compile companion `ui-new` / `DISPLAY_CLASS`.
- Keep Access OLED as home (READY / USER menu / 10 s DISPLAYOFF). Optional READY line may show `M485` or baud. No MeshCore chat.
- **Never** `UART byte → one LoRa/Companion packet`. A MESH 485 request carries a whole RS-485 **frame** (base64) and a **timeout**.
- Photos never in MESH 485.
- Fire CONTROL / Bolid opcodes: out of scope.
- Node must keep working if the host disconnects (dispatcher is local).

## Hard constraint: Serial is Companion

After `mesh.start()`, USB CDC (this workspace: **COM23**) is MeshCore Companion. Dumping JSON there will break `meshcore-cli` / official apps.

**Do not** print MESH 485 responses as raw JSON on the companion stream unless they are isolated as below.

### Required host path (USB)

Companion frames never start with ASCII `{` (`0x7B`). Implement a thin **USB mux** in *our* code (not in `vendor/`):

- If the first byte of a USB burst/frame is `{`, accumulate a line (max ~512 bytes, until `\n` or timeout ~50 ms), run the dispatcher, write **one JSON response + `\n`** back to USB, and **do not** feed those bytes into `ArduinoSerialInterface`.
- All other bytes pass through to Companion unchanged.

If wrapping `Serial` as a `Stream` for `usb_serial_interface.begin(...)` is too invasive, document why and fall back to:

1. Golden **native** tests (mandatory either way), and
2. A second PlatformIO env `heltec_v4_mesh485_json` **without** `ENABLE_USB_INTERFACE` (BLE companion stays on the main env). Production env `heltec_v4_access` must remain **BLE+USB companion**.

Prefer the mux so one image stays AGENTS-compliant.

BLE companion: do not mux JSON on BLE this run (binary NUS). Local USB mux is enough to prove the dispatcher.

## Deliverables

### 1. Spec freeze (docs + examples)

Keep `docs/mesh485-protocol.md` as source of truth. Add:

- `protocol/mesh485/examples/node_status.request.json`
- `protocol/mesh485/examples/node_status.response.json`
- `protocol/mesh485/examples/rs485_transaction.request.json`
- `protocol/mesh485/examples/diagnostics.response.json`
- `protocol/mesh485/README.md` — how to send a line on USB mux; node id; replay rules

Envelope (must match):

```json
{
  "v": 1,
  "type": "m485.request",
  "id": "8A12F2",
  "node": "EXT-001",
  "op": "NODE_STATUS",
  "operator": "admin",
  "ts": 1787912345,
  "nonce": "c1f0",
  "payload": {}
}
```

Response:

```json
{
  "v": 1,
  "type": "m485.response",
  "id": "8A12F2",
  "node": "EXT-001",
  "op": "NODE_STATUS",
  "result": "OK",
  "error": null,
  "ts": 1787912347,
  "payload": {}
}
```

Errors: `REPLAY`, `BAD_AUTH` (stub: ignore auth this run), `UNKNOWN_CMD`, `BUSY`, `TIMEOUT`, `HARDWARE`, `NO_PERMISSION`.

Replay: reject duplicate `(id, operator)` within TTL (e.g. 30 s) or `ts` older than a window. Simple RAM table of last N ids is enough.

Node id string: `EXT-001` in MESH 485 (OLED may stay `TS-001`).

### 2. Firmware module `src/mesh485/` + `include/mesh485/`

Suggested split (names can vary, keep it small):

| File | Job |
|---|---|
| `codec.*` | Parse/serialize JSON envelope (no Arduino `String` soup if possible; ArduinoJson **or** a tight manual parser for this subset) |
| `dispatcher.*` | `handle(request) → response` |
| `replay.*` | id window |
| `usb_mux.*` | optional Stream mux (if used) |

Do **not** use `mesh_access/map_protocol.cpp` strstr as the parser.

If you add ArduinoJson, pin a version in `platformio.ini` `lib_deps`. Prefer not to pull huge libs if a 200-line parser covers `v`, `type`, `id`, `node`, `op`, `operator`, `ts`, `nonce`, `payload.timeout_ms`, `payload.frame`, `payload.protocol`.

### 3. Ops to implement (only these)

| op | Behavior |
|---|---|
| `NODE_STATUS` | `radio` (ok/error), `repeater`, `uptime_s`, `profile` (`access+rs485`), `rs485.ok`, `rs485.baud`, `rs485.protocol` (`RAW`), `rs485.rx`, `rs485.tx` |
| `CONFIG_GET` | baud, parity (`N`/`E`/`O`), stop, protocol placeholder `RAW`, mode `MONITOR`. RAM defaults matching HAL (9600 8N1). **Do not** persist NVS this run. |
| `DIAGNOSTICS` | Mesh RSSI/SNR/RX/TX if available; RS-485 rx/tx counts; `last_error`; heap |
| `RS485_TRANSACTION` | payload `{ "protocol": "RAW_RS485" or "MODBUS_RTU" (treat same this run), "timeout_ms": 900, "frame": "<base64>" }`. Decode frame, `writeFrame`, `readFrame` until timeout or first gap (document: RAW = read whatever arrives until `timeout_ms`). Response payload: `{ "mesh_ok": true, "tx_ok": bool, "device_ok": bool, "frame": "<base64 or empty>", "error": null or "TIMEOUT" }` |

`mesh_ok`: request reached this dispatcher (always true if we answer).  
`tx_ok`: UART write succeeded.  
`device_ok`: at least one response byte (or you define Modbus-like “got a frame”; for RAW, any RX > 0).

**Refuse** (result not OK, `error: UNKNOWN_CMD` or `NO_PERMISSION`): `BRIDGE_OPEN`, `BRIDGE_CLOSE`, `COMMAND_EXECUTE`, `DEVICE_*`, `POINT_*`, `EVENT_*`, `CONFIG_SET`, `ACCESS_*`.

### 4. Replace Phase 05 byte-echo

In `main.cpp` **stop** automatically echoing every RS-485 RX byte back onto the bus (that will fight real instruments).

Idle RX may still update OLED `RS485 N B`. Do not TX unless `RS485_TRANSACTION` (or a future bridge).

Keep `mesh.loop()` / OLED / Wiegand / turnstile cooperative. `RS485_TRANSACTION` may block up to `timeout_ms` (cap e.g. 2000 ms) — do not starve the loop for seconds without a cap.

### 5. Tests

Extend `[env:native_protocol_tests]` (or add `native_mesh485`) so it compiles **without** Arduino HAL:

- Parse golden request → dispatch with a **fake** RS-485 (in-memory loopback) → serialize response.
- Replay rejection.
- Unknown op.
- `RS485_TRANSACTION` fake: write 3 bytes, fake reads them back, base64 round-trip.

`pio run -e native_protocol_tests` (or the new env) must succeed.

### 6. On-device proof

- Build/flash `heltec_v4_access` to **COM23** (existing `upload_port`).
- Boot: OLED READY, USER menu still works, 10 s blank still works.
- If USB mux exists: send one NDJSON `NODE_STATUS` line from a short Python/`pio device monitor` script; expect JSON `m485.response`. Then confirm Companion still enumerates (optional: meshcore-cli `device query` if installed).
- `RS485_TRANSACTION` with empty bus → `device_ok: false`, `error: TIMEOUT` is success.
- If no isolator/dongle: write that blockage in `docs/implementation.md`; do not fake a device ACK.

### 7. Docs

- `docs/implementation.md` Phase 06.
- `firmware/heltec_v4/README.md`: how to send `NODE_STATUS`; echo removed; mux vs companion.
- Update `docs/mesh485-protocol.md` if the implemented JSON differs (keep it true).

## Forbidden this run

- Bolid / Orion / Modbus exception parsing (RAW bytes only).
- Flutter, FastAPI CRUD, photos, OTA, Excel.
- MeshCore chat UI, contacts list on OLED.
- Editing `vendor/`.
- `BRIDGE_*` session state (prompt 07).
- Changing Access GPIO map.

## Done when

1. Golden examples exist and native tests pass.
2. Dispatcher implements the four ops; others return a clean error envelope.
3. `RS485_TRANSACTION` uses `Rs485Hal` (not a second UART).
4. Phase 05 bus echo is gone.
5. OLED Access UI + companion BLE+USB image still build (`pio run -e heltec_v4_access`).
6. Phase 06 note in `docs/implementation.md`.

Then **stop**. Next cascade file: `agent/prompts/07-remote-bridge.md`.
