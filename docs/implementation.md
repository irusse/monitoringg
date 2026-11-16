# Implementation (trimmed)

Product: **MESH 485 Extender** (ADR 0005). Phases 01–23 are **done** (Access/turnstile + RS-485 HAL + protocol + READ_ONLY bridge + DeviceAdapter + Orion CRC freeze + Path A С2000-ПП Modbus stub + event queue / dual ACK + Flutter console + live USB/BLE operator screens + RU UI + capability-gated CONTROL + Signal-10/20 MONITOR + offline ACL + Access employees UI + hardening audit + local AuthZ + allow-list persist + optional pubkey bind + **persist CONFIG protocol**). Cascade 01–19 closed. Phases 20–23 are post-cascade.

## Phase 01 (FIRST_RUN) — done

Pinout frozen in `docs/pinout.md`, `firmware/heltec_v4/include/access/pins.h`, `hardware/wiring.md` (D0/D1 15/16, OPEN 40, lamps 41/33, ENTRY/EXIT/ESTOP 3/4/6, OLED 17/18/21, USER 0, USB 19/20 unused). Silkscreen exception: Arduino `SDA/SCL` are 3/4; OLED uses `SDA_OLED` 17/18.

SSD1306 Access UI (Adafruit, VEXT GPIO 36) in `firmware/heltec_v4/src/display/access_display.cpp`: READY / GRANTED / DENIED / REMOTE / LOCKED / MENU (Info/Back). Not MeshCore splash or chat. `main.cpp` calls `oled.begin()` and `showReady("TS-001", ...)`. USER short-press demo-cycles screens; long-press opens MENU.

Build: `cd firmware/heltec_v4 && pio run -e heltec_v4_access` then `-t upload`.

## Phase 02 — MeshCore BLE+USB companion

`heltec_v4_access` links MeshCore companion (`BLE_PIN_CODE` + `ENABLE_USB_INTERFACE` via `MultiSerialInterface`) using `heltec_v4_oled` radio/OLED flags. Access SSD1306 still owns the panel (no `DISPLAY_CLASS`, no `ui-new`). Serial banner: `3MESH ACCESS`, BLE name `3MESH-TS-001`, `USB CDC ready`. `M*` follows `radio_init()`. Repeater default on, `setRepeaterEnabled(bool)` persists. No Wiegand ACL in this phase.

## Phase 03 — Access OLED complete

All `docs/oled-ui.md` screens on SSD1306 (Adafruit, VEXT 36 **LOW = on**, I2C 17/18/21): READY, GRANTED (name/direction/time), DENIED, REMOTE OPEN, LOCKED, EMERGENCY, MENU (Info / Repeater ON|OFF / Back), INFO. Indicator is `M`+filled/empty circle or `M!`, plus `R` when forwarding. USER short-press demos screens; long-press opens menu and can toggle repeater via `MeshCoreService`. No MeshCore chat/QR/photos.

Blank-screen bug: Access used MeshCore `PIN_VEXT_EN_ACTIVE=HIGH`, which cuts the V4 PMOS rail (~1.6 V). Firmware now holds GPIO 36 LOW (same as stock MeshCore companion, which never `claim()`s VEXT). OLED is re-inited after `mesh.start()` I2C scan. USER is GPIO 0, polled after OLED I2C timeout is set so a wedged bus cannot starve the button.

## Phase 04 — Wiegand + turnstile

WiegandNG (`vendor/esp32-rfid` usage: 58-bit buffer, 25 ms gap) on D0/D1 **15/16** → `CardEvent`. 26/34 (24–58) PICC accepted. Stub ACL allowed any UID until prompt 17 (offline ACL, DENY default). GRANT → OLED `showGranted` + GPIO 40 OPEN pulse + green lamp; DENY → `showDenied` + red. Turnstile SM: LOCKED / OPENING / OPEN / CLOSING / ERROR / EMERGENCY. ENTRY/EXIT GPIO 3/4, E-STOP GPIO 6 active LOW. Level shifter still required on D0/D1.

## Phase 05 — Isolated RS-485 HAL

UART1 on GNSS silk: TX **39**, RX **38**, DE **26** (HIGH = TX, idle RX). Frozen in `docs/pinout.md`, `pins.h`, `hardware/rs485-adapter.md`. `ENV_INCLUDE_GPS=0` so MeshCore does not claim Serial1.

`Rs485Hal`: 9600 8N1 default, configurable baud/parity/stop, `writeFrame` / `read` / `readFrame(timeout)`, DE around TX + flush. Phase 05 echoes RX back on the bus and shows `RS485 N B` on READY. USB CDC (COM23) stays MeshCore companion — echo is on RS-485 + OLED, not companion bytes.

OLED: DISPLAYOFF after 10 s idle; USER wakes (press swallowed). GRANT/DENY/REMOTE/EMERGENCY wake. VEXT stays on.

Upload port: **COM23**.

Isolated transceiver on a live Orion bus is still required; bench may use a USB-RS485 dongle on A/B.

## Phase 06 — MESH 485 protocol (JSON + RAW transaction)

Application envelope in `src/mesh485/` (not the 3MAP `strstr` toy parser). Golden examples: `protocol/mesh485/examples/`. Node id **`EXT-001`**.

Ops: `NODE_STATUS`, `CONFIG_GET` (RAM 9600 8N1, mode `MONITOR`, no NVS), `DIAGNOSTICS`, `RS485_TRANSACTION` (base64 frame + timeout, uses existing `Rs485Hal`). Phase 17 adds `ACCESS_*`. Earlier `BRIDGE_*` / `CONFIG_SET` / `DEVICE_*` refusals were replaced in 07–16.

USB mux (`UsbMuxStream`): `{` NDJSON on COM23 is MESH 485; everything else is Companion. `python firmware/heltec_v4/scripts/m485_node_status.py COM23`. BLE not muxed.

Phase 05 bus **echo removed**. Idle RX still updates OLED `RS485 N B`; READY line may show `M485 9600`. TX only from `RS485_TRANSACTION` (or a future bridge). RAW read: gap after first byte or `timeout_ms` (cap 2000 ms).

Empty A/B (no isolator/dongle): `device_ok: false`, `error: TIMEOUT` is the correct result — do not fake a device ACK. Isolated transceiver still required on a live bus.

Native: `pio run -e native_protocol_tests` (Windows without MinGW uses MSVC via `extra_native_msvc.py`). Device image: `pio run -e heltec_v4_access`. This run built the image (RAM 8.2% / flash 18.6%); `pio run -t upload` on COM23 hit PermissionError (port held or 1200 bps re-enum). Close any serial monitor and upload when COM23 is free. Isolated A/B still required for a live instrument ACK — empty bus `TIMEOUT` is the correct result, not a fake device ACK.

## Phase 07 — Remote RS-485 bridge (READ_ONLY default)

Service-port session on the existing HAL (`src/mesh485/bridge.cpp` + dispatcher). Not MeshCore `WITH_RS232_BRIDGE`, not UART-byte-per-packet.

- `BRIDGE_OPEN`: omit `level` → `READ_ONLY`; no TX / no `RS485_TRANSACTION`. `CONTROL` → `NO_PERMISSION` unless `-D MESH485_ALLOW_BRIDGE_CONTROL=1` (still no `COMMAND_EXECUTE`). `RAW_BRIDGE` must be explicit; then Phase 06 transactions are allowed. Unknown `level` → `UNKNOWN_CMD`.
- One session; second OPEN → `BUSY`. `BRIDGE_CLOSE` is idempotent. Idle **30 s** (payload `idle_s`) auto-closes and leaves DE **RX**. Authz is an ADMIN stub.
- `BRIDGE_POLL` (`BRIDGE_DRAIN` alias) drains an 8×256 RAM ring as base64 frames; empty `frames: []` is OK. No unsolicited NDJSON. Capture is whatever `rs485.read()` returned this loop.
- OLED `BRIDGE` + level; USER long-press aborts; panel stays awake while the session is open. READY may show `BR RO` / `BR RAW`.
- USB mux unchanged (`{` → dispatcher). `NODE_STATUS.session` is `"off"` or `{level, idle_s remaining}`. `CONFIG_GET.mode` is `MONITOR` vs `BRIDGE`.
- Isolated A/B still required on a live bus. Empty bus: POLL empty is success; do not fake instrument ACKs. Session is RAM; reboot → MONITOR.

Native: `pio run -e native_protocol_tests` → `native_protocol_tests: ok`. Device image: `pio run -e heltec_v4_access` succeeded (RAM 8.3% / flash 18.7%). `pio run -t upload` on COM23 failed: 1200 bps re-enum then `PermissionError` (CDC held — same as Phase 06). Close any serial monitor and upload when COM23 is free, then `python scripts/m485_bridge.py COM23 open|poll|txn|close`. Isolated A/B still required for a live instrument ACK — empty bus POLL `frames: []` is success, not a fake device ACK.

## Phase 08 — DeviceAdapter framework (Device / Point / Event)

`DeviceRegistry` plus two RAM stubs: `RawRs485Adapter` (`RAW-001` / `RAW_RS485`) and `AccessTurnstileAdapter` (`TS-001` / `ACCESS`). Clients see Device / Point / Event, never Bolid opcodes. No Orion parser, no Signal-10, no `COMMAND_EXECUTE`, no NVS protocol list, no AUTO discovery.

Ops: `DEVICE_DISCOVER` / `DEVICE_INFO` / `DEVICE_STATUS` / `POINT_LIST` / `POINT_STATUS` / `EVENT_LIST`. Unknown `device_id` → `UNKNOWN_CMD`. `EVENT_SUBSCRIBE` → `NO_PERMISSION`. `COMMAND_EXECUTE` still refused. Raw adapter does **not** TX and does not parse Modbus. Access stub maps `TurnstileHAL` state + last Wiegand UID; OPEN is not exposed. `NODE_STATUS` includes `"devices":2`. `CONFIG_GET` protocol remains `RAW`.

USB mux unchanged (`{` → dispatcher). `python firmware/heltec_v4/scripts/m485_devices.py COM23 discover`. Empty bus: do not fake Bolid events (`EVENT_LIST` `"events": []` is OK). READY OLED still `M485 9600` / `BR RO` (device count is in `NODE_STATUS`, not the 64 px row). Bridge abort and 10 s blank unchanged.

Golden examples: `protocol/mesh485/examples/device_*.json`, `point_list.response.json`, `event_list.response.json`.

Native: `pio run -e native_protocol_tests` → `native_protocol_tests: ok`. Device image: `pio run -e heltec_v4_access` succeeded (RAM 8.3% / flash 18.7%). Upload to **COM23** succeeded. USB mux (`python scripts/m485_devices.py COM23 all`): `NODE_STATUS` `"devices":2`, `DEVICE_DISCOVER` returns `RAW-001` + `TS-001`, `DEVICE_INFO` `RAW_RS485` + `READ_STATUS`, `POINT_LIST` has BUS/TURNSTILE/READER, `EVENT_LIST` `"events":[]` (empty bus, no fake Bolid), `COMMAND_EXECUTE` `NO_PERMISSION`. `{` still muxed; Companion boot lines are skipped by the script (re-opening CDC often resets the S3).

## Phase 09 — Orion / Bolid research freeze (CRC + docs)

No Bolid `DeviceAdapter`, no Modbus poller, no `COMMAND_EXECUTE`, no CONTROL on A/B. CRC is a native-tested module only — **not** wired to `RS485_TRANSACTION` (still opaque RAW / gap-framed). Registry stubs stay `RAW-001` / `TS-001`. `CONFIG_GET` `"protocol":"RAW"`.

- Frame layout + worked hex: `docs/reference/orion-frames.md`. CRC is **last byte**; length = `total − 1`. Address 1..127 (encrypted dumps use `addr|0x80`).
- **`crc8_orion` = CRC-8/MAXIM-DOW** (Dallas/Maxim, poly 0x31 reflected / 0x8C). Fixtures from Habr 563584 (cites CXEM topic 105126). Dallas/Maxim **matches**; generic poly 0x07 **mismatches**. Code: `include/protocols/bolid/crc8.h`, `src/protocols/bolid/crc8.cpp`. Vectors: `test/native/orion_vectors.h`, `docs/reference/orion-vectors/`.
- Timings: `docs/reference/orion-timings.md`. `RAW_BRIDGE` is not the Orion path (mesh RTT ≠ 50 ms UART turnaround). HAL gap unchanged (≈5 ms at 9600).
- hass-hubc2000pp cloned to `vendor/hass-hubc2000pp`. Names only: `docs/reference/hubc2000pp-entities.md`.
- Path for prompt 10: **Path A** (product default) + empty-bus Modbus **read stub** that times out (`docs/reference/orion-path-decision.md`). С2000-ПП read map: `docs/reference/c2000pp-modbus.md` (АЦДР.426469.020 РЭп Изм.17).
- `docs/bolid.md` updated to the matched CRC.

Native: `pio run -e native_protocol_tests` → `native_protocol_tests: ok` (Phase 06–08 cases + Orion CRC fixtures). Device image: `pio run -e heltec_v4_access` succeeded (RAM 8.3% / flash 18.7%). CRC-only — **no flash**. Isolated A/B still required on a live bus.

## Phase 10 — С2000-ПП Modbus READ_ONLY (`PP-001`)

Path A stub (`docs/reference/orion-path-decision.md`). Third adapter `C2000ppModbusAdapter` on the same `Mesh485Port` as RAW. Heltec is Modbus **client**, slave **1**, UART still **9600 8N1**. Probe: FC **0x03** start **46152** qty **2** (АЦДР.426469.020 РЭп table 1.1.5.14.1), CRC-16 RTU (`protocols/modbus/crc16.*`). Timeout 900 ms, poll interval ~5 s, at most one in-flight frame. Empty bus / flipped CRC → `online: false`, point `PP` `FAULT`, `EVENT_LIST` `[]`. Valid FC03 (type 36 in tests) → `online: true`. No write coils, no IR 3600, no `crc8_orion` on these frames, `COMMAND_EXECUTE` still `NO_PERMISSION`. `READ_ONLY` / `CONTROL` session: adapter does **not** `writeFrame`. `CONFIG_GET` protocol remains `RAW`. `NODE_STATUS.devices` = 3. OLED READY shows `PP-001 FAULT` (offline).

Native: `pio run -e native_protocol_tests` → `native_protocol_tests: ok`. Device image: `pio run -e heltec_v4_access` succeeded (RAM 8.3% / flash 18.7%). Upload to **COM23** succeeded. USB mux (`python scripts/m485_devices.py COM23 all`): `NODE_STATUS` `"devices":3`, `DEVICE_DISCOVER` returns `RAW-001` + `TS-001` + `PP-001` (`online: false`), `DEVICE_INFO` `PP-001` `BOLID_C2000PP` + `READ_STATUS`, diagnostic point `PP` `FAULT`, `EVENT_LIST` `"events":[]`, `COMMAND_EXECUTE` `NO_PERMISSION`. Isolated A/B still required on a live converter — empty bus TIMEOUT / offline is the correct result.

## Phase 11 — Event queue and dual ACK

Persistent ring (`EventStore`, cap 32, drop oldest) on **SPIFFS** `/m485_evt.bin` — same FS as MeshCore `DataStore` (not LittleFS). Records: UUID + sequence + HAPRS-like columns + MESH 485 Event fields + **`mesh_ok` vs `device_ok`**. Photos never stored. Access card events enqueue with `source=RFID` (ACL still stub). PP-001 empty bus still does not enqueue FIRE / type 36; optional DIAG only after a CRC-valid probe.

`EVENT_LIST` returns the queue (`since` = seq or UUID). `EVENT_SUBSCRIBE` is allowed on the USB mux companion pipe; firmware writes NDJSON `m485.event` (one per `loop()`, retry until USB write ACK or give up with visible pending). Do not UART-byte-stream events over LoRa. Lab inject: `python scripts/m485_devices.py COM23 inject` (`stub: CARD`). `DIAGNOSTICS` surfaces both ACK flags. OLED READY may show `Q:n` pending. `NODE_STATUS.devices` stays 3. `CONFIG_GET` protocol stays `RAW`. Queue flush is non-blocking. Loop task stack is 16 KiB (`CONFIG_ARDUINO_LOOP_STACK_SIZE`) so JSON + harvest do not trip the canary.

Native fake-FS reboot is covered by `event_store_use_fake_fs` in `native_protocol_tests`. Device SPIFFS survives power cycle.

Native: `pio run -e native_protocol_tests` → `program.exe` → `native_protocol_tests: ok`. Device image: `pio run -e heltec_v4_access` succeeded (RAM 8.8% / flash 18.8%). Upload to **COM23** succeeded. USB mux (`python scripts/m485_devices.py COM23 inject`): `EVENT_SUBSCRIBE` `subscribed:true`, `EVENT_LIST` returns a CARD (`source=RFID`, `rfid_uid=STUBCARD`, `mesh_ok`/`device_ok` both present), `DIAGNOSTICS` shows separate `mesh_ok` vs `device_ok` plus `events.pending_mesh`. Empty PP bus still has no FIRE.

## Phase 12 — Flutter console shell

One package `app/mesh485` (Windows + Android + web). Same screens on both: Dashboard, Objects, Devices, Events, Mesh, Bridge, Access, Settings. Login is a stub `viewer` chip.

Transport is **stubbed**: `Mesh485Client` + `FixtureMesh485Client` (in-memory MESH 485 JSON). Device ids match firmware: `RAW-001` / `TS-001` / `PP-001` (`PP-001` `online: false`). Events are fixture rows with `mesh_ok` vs `device_ok` (not live `EVENT_SUBSCRIBE`). Mesh is extender health (RSSI/SNR/RX/TX/uptime) — not MeshCore chat. Bridge is a `READ_ONLY` / off placeholder (no live `BRIDGE_OPEN`). Access is a `TS-001` card (no OPEN / employees). `COMMAND_EXECUTE` / Control is prompt 14.

Do not vendor `vendor/meshcore-open` UI. Live USB CDC (Windows) and BLE (Android) are prompt 13.

Build: `C:\flutter\bin\flutter.bat test` in `app/mesh485` → 7 tests passed. `flutter run -d windows` launched the shell:

```
Launching lib\main.dart on Windows in debug mode...
Building Windows application...                                    47,9s
√ Built build\windows\x64\runner\Debug\mesh485.exe
Syncing files to device Windows...                                 106ms
Flutter run key commands.
A Dart VM Service on Windows is available at: http://127.0.0.1:58397/...
```

`flutter devices` showed Windows, Chrome, Pixel 6a (`26201JEGR02986`), and M2101K6G.

`flutter run -d 26201JEGR02986` installed and launched:

```
√ Built build\app\outputs\flutter-apk\app-debug.apk
Installing build\app\outputs\flutter-apk\app-debug.apk...
Flutter run key commands.
A Dart VM Service on Pixel 6a is available at: http://127.0.0.1:50634/...
```

`flutter run -d chrome --web-port=8090` also launched. Live USB/BLE remains prompt 13. Firmware unchanged.

## Phase 13 — Live operator console + Russian UI

Windows USB CDC is the live transport (`LiveMesh485Client` / `usb:COM23`). Opening the port still resets the S3; the client waits ~2.5 s and skips non-`{` Companion/boot lines (same as `scripts/m485_devices.py`). Settings shows the active transport; no port / timeout → fixture JSON.

Android talks Nordic UART to `3MESH-TS-001` **without** Companion `0x3c`/`0x3e` wrapping. Firmware `BleMuxInterface` clones the USB `{` mux (first byte `{` → MESH 485; responses chunked to `MAX_FRAME_SIZE`). Pairing PIN is MeshCore **123456** (system dialog). USB-OTG remains the fallback if pairing fails.

Operator screens bind VIEWER ops: `NODE_STATUS`, `DIAGNOSTICS`, `DEVICE_DISCOVER`, `DEVICE_INFO`, `DEVICE_STATUS`, `POINT_LIST`, `EVENT_LIST`, optional `EVENT_SUBSCRIBE`. Operator actions this run: `COMMAND_EXECUTE` (Control) and `ACCESS_OPEN` (Access). Dual ACK stays two bools (`mesh_ok` / `device_ok`, labeled сеть / прибор). PP-001 empty bus is `online: false` / point `FAULT`.

Flutter l10n: `flutter gen-l10n`, default locale `ru`. OLED: `font6x8_cyr.h` + `oledPrintUtf8`; mockups in `docs/oled-ui.md`. Protocol ids stay Latin.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM 9.2% / flash 18.9%). Upload to **COM23** succeeded.

USB mux after flash (`python scripts/m485_devices.py COM23 discover`):

```
{"v":1,"type":"m485.response","id":"DV0001","node":"EXT-001","op":"DEVICE_DISCOVER","result":"OK","error":null,"ts":1787912345,"payload":{"devices":[{"id":"RAW-001","protocol":"RAW_RS485","online":true},{"id":"TS-001","protocol":"ACCESS","online":true},{"id":"PP-001","protocol":"BOLID_C2000PP","online":false}]}}
```

`C:\flutter\bin\flutter.bat test` in `app/mesh485` → 7 tests passed (RU strings / l10n). `flutter run -d windows`:

```
Launching lib\main.dart on Windows in debug mode...
Building Windows application...                                    45,6s
√ Built build\windows\x64\runner\Debug\mesh485.exe
Syncing files to device Windows...                                 307ms
Flutter run key commands.
A Dart VM Service on Windows is available at: http://127.0.0.1:57263/...
```

Android devices present (Pixel 6a `26201JEGR02986`, M2101K6G). BLE path: scan `3MESH-*`, NUS NDJSON, PIN 123456.

## Phase 14 — Allowed CONTROL + capabilities

`COMMAND_EXECUTE` is OPERATOR+ and only if `Device.capabilities` contains the payload `capability`. Executable allow-list this run: **`CONTROL_RELAY`** on named points `R1`…`R8` when panel config allows that relay (default **empty** — no TX on the live empty bus). Forbidden: `DISABLE_FIRE_ALARM` / `DISARM_ALL` / silence-all (always `NO_PERMISSION`). Dual ACK: `mesh_ok` vs `device_ok` vs `TIMEOUT`. RAM audit: `DIAGNOSTICS.last_command`. Flutter Control screen confirms then shows the three ACK chips. Fixture advertises `CONTROL_RELAY`+`R1` for UI; firmware default does not.

Roles: envelope `operator` `viewer` | `operator` | `admin`. ACCESS OPEN stays 17–18. RAW-001 / TS-001 do not advertise `CONTROL_RELAY`. Write path (when allowed): Modbus FC05 coil `0x2710+(n-1)`.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM 9.2% / flash 18.9%). Upload to **COM23** succeeded.

USB mux after flash (`python scripts/m485_devices.py COM23 all`):

```
PP-001 capabilities ["READ_STATUS"] (no CONTROL_RELAY — default allow-list empty)
COMMAND_EXECUTE {} → NO_PERMISSION mesh_ok:true device_ok:false
COMMAND_EXECUTE DISABLE_FIRE_ALARM → NO_PERMISSION mesh_ok:true device_ok:false
COMMAND_EXECUTE CONTROL_RELAY R1 ON → NO_PERMISSION (cap not advertised)
```

`C:\flutter\bin\flutter.bat test` in `app/mesh485` → 10 tests passed (roles, Control confirm, dual ACK timeout, no fire-disable button). `flutter run -d chrome --web-port=8091`: login roles наблюдатель/оператор/администратор → Управление → PP-001 CONTROL_RELAY R1 → confirm → `mesh_ok: true` / `device_ok: false` / `таймаут: true` / `error: TIMEOUT`. Next: `agent/prompts/15-signal10.md`.

## Phase 15 — Signal-10 adapter (MONITOR first)

Fourth RAM adapter `Signal10Adapter` (`S10-001` / `BOLID_SIGNAL10`) beside `PP-001`. Path B Orion **status-read** from Habr 563584 (`0x57`/`0x02` encrypted, CRC-8/MAXIM). Default address **1**. Empty bus → `online: false`, point `S10` `FAULT`, no FIRE, no `Z1`. CRC-valid reply → `online: true`, `READ_ZONES`, `Z1` `UNKNOWN` (no zone bitmap in dumps). **No CONTROL** — prompt 14 `CONTROL_RELAY` stays on PP-001 only; dumps do not justify a Signal-10 relay opcode.

Manual `CONFIG_SET` `protocol` (ADMIN, RAM, no NVS): `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10`. **`AUTO` refused.** Default `RAW` keeps the С2000-ПП probe; `BOLID_SIGNAL10` moves UART TX to this adapter. `READ_ONLY` session still blocks adapter TX. OLED READY shows `S10-001 FAULT` when that protocol is selected, else `PP-001 …`.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM 9.2% / flash 19.0%). Upload to **COM23** succeeded.

USB mux (`python scripts/m485_devices.py COM23 all`): `NODE_STATUS` `"devices":4`, `DEVICE_DISCOVER` returns `RAW-001` + `TS-001` + `PP-001` + `S10-001` (both Bolid `online: false`), `CONFIG_GET` `protocol: RAW` and `protocols` without AUTO, `S10-001` `READ_STATUS` only, point `S10` `FAULT`, `EVENT_LIST` `[]`, `COMMAND_EXECUTE` `NO_PERMISSION`. `CONFIG_SET` `AUTO` → `UNKNOWN_CMD` / `no AUTO`. `CONFIG_SET` `BOLID_SIGNAL10` → RAM OK (lost on CDC re-open/reset).

## Phase 16 — Signal-20 adapter (MONITOR first)

Fifth RAM adapter `Signal20Adapter` (`S20-001` / `BOLID_SIGNAL20`) beside `S10-001`. Path B Orion **status-read** shared with Signal-10 (`orion_build_status_request`, Habr 563584 `0x57`/`0x02` encrypted, CRC-8/MAXIM). No Signal-20-specific dump in `orion-vectors/`. Default address **1**. Empty bus → `online: false`, point `S20` `FAULT`, no FIRE, no `Z1`. CRC-valid reply → `online: true`, `READ_ZONES`, `Z1` `UNKNOWN` (no 20-zone bitmap). **No CONTROL** — dumps do not justify a native Orion relay opcode. CXEM Signal-20 relay examples stay a caveat: CRC OK ≠ contact closed (relay **program**).

Manual `CONFIG_SET` `protocol` (ADMIN, RAM, no NVS): `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10` | `BOLID_SIGNAL20`. **`AUTO` refused.** Default `RAW` keeps the С2000-ПП probe; `BOLID_SIGNAL20` moves UART TX to this adapter. `READ_ONLY` session still blocks adapter TX. OLED READY shows `S20-001 FAULT` when that protocol is selected.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM 9.2% / flash 19.0%). Upload to **COM23** succeeded.

USB mux (`python scripts/m485_devices.py COM23 all`): `NODE_STATUS` `"devices":5`, `DEVICE_DISCOVER` returns `RAW-001` + `TS-001` + `PP-001` + `S10-001` + `S20-001` (Bolid `online: false`), `CONFIG_GET` lists `BOLID_SIGNAL20` without AUTO, point `S20` `FAULT`, `EVENT_LIST` `[]`. `CONFIG_SET` `AUTO` → `UNKNOWN_CMD` / `no AUTO`. `CONFIG_SET` `BOLID_SIGNAL20` → RAM OK; `COMMAND_EXECUTE` `CONTROL_RELAY` on `S20-001` → `NO_PERMISSION`.

## Phase 17 — Access ACL + 3MAP

Offline ACL replaces the phase-04 GRANT-any-UID stub. Lookup is canonical uppercase hex (no spaces). Empty store / unknown / disabled → **DENY** (red lamp, `showDenied`, no OPEN pulse, event `CARD` / `Denied`). Hit enabled → ALLOW (green + GPIO 40 pulse, `showGranted` with name or UID). Persist SPIFFS `/m485_acl.bin` (cap 64, overflow `BUSY`, no silent wrap). Lab: `AccessController::allowUid` / `-D MESH485_ALLOW_TEST_UID=1` (`04A79122`). UID is not crypto.

MESH 485 `ACCESS_STATUS` (VIEWER), `ACCESS_OPEN` / `ACCESS_TURNSTILE_OPEN` (OPERATOR+, dual ACK, OLED `showRemote`, event `REMOTE_OPEN`), `ACCESS_LOCK` (OPERATOR+), `ACCESS_GET_EVENTS` (same `EventStore`, TS-001), `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD` (ADMIN). VIEWER mutate/OPEN → `NO_PERMISSION`. `ACCESS_OTA` / fire-disable names → `UNKNOWN_CMD`. `COMMAND_EXECUTE` is **not** OPEN. `ACCESS_*` does not TX UART1. TS-001 advertises `READ_STATUS` + `ACCESS_OPEN` (executable only via `ACCESS_OPEN` op). INFO users = enabled ACL count. Five DISCOVER ids unchanged.

Flutter Access: OPERATOR+ OPEN confirms then calls live `ACCESS_OPEN`. VIEWER button stays disabled. Fixture ACK is honest TIMEOUT (no GPIO). Employees/photos stay prompt 18.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM 9.2% / flash 19.1%; ACL records on heap so BSS stays modest). Upload to **COM23** succeeded. First images with inline ACL BSS panicked in C2000-PP `poll` → `Serial1.write`; heap ACL boots stably (uptime >2 min, BLE GATT noise only).

USB mux (`python scripts/m485_devices.py COM23 all` plus ACCESS extras): `NODE_STATUS` `"devices":5`, DISCOVER `RAW-001` + `TS-001` + `PP-001` + `S10-001` + `S20-001` (Bolid `online: false` on empty bus). `TS-001` capabilities `READ_STATUS` + `ACCESS_OPEN`. Empty ACL `ACCESS_STATUS` `users:0`. Admin `ACCESS_OPEN` dual ACK (`mesh_ok`/`device_ok` true, `state: OPEN`). VIEWER `ACCESS_OPEN` / `ACCESS_LOCK` / `ACCESS_ADD_CARD` → `NO_PERMISSION`. ADMIN add `DEADBEEF` → `users:1`, remove → `users:0`. `ACCESS_GET_EVENTS` includes `REMOTE_OPEN` UUID `source=USB`. `ACCESS_OTA` `UNKNOWN_CMD`. `COMMAND_EXECUTE` `ACCESS_OPEN` still `NO_PERMISSION`. Isolated A/B empty-bus S20 still offline.

## Phase 18 — Access employees UI

Same Flutter console: employees + Wiegand cards against the **node** ACL, photos **from disk only**, OPERATOR+ OPEN and LOCK, live TS-001 events.

- Identity: `employee_id` + `name`. UID canonical uppercase hex. Add/update → `ACCESS_ADD_CARD`; remove → `ACCESS_REMOVE_CARD`; overflow `BUSY`. Unknown UID still DENY (UI does not fake GRANT).
- ADMIN `ACCESS_GET_USERS` dumps `{uid, enabled, employee_id?, name?}` (page 16, cap 64, **no photo bytes**). VIEWER/OPERATOR → `NO_PERMISSION`; VIEWER count via `ACCESS_STATUS.users`.
- Photos: local path next to the employee in Flutter. Never in MESH 485 JSON, EventStore, SPIFFS, OLED, or LoRa. FastAPI `/employees` stays `{items:[]}` (Postgres CRUD follow-up).
- OPEN remains `ACCESS_OPEN` (confirm). LOCK is `ACCESS_LOCK` (confirm). Not `COMMAND_EXECUTE`. Fixture OPEN/LOCK = honest TIMEOUT. ADMIN add-card succeeds in fixture RAM (not a GPIO pulse).
- Events: Access tab tails `ACCESS_GET_EVENTS` / snapshot `EVENT_LIST` for TS-001 (UUID, uid, Authorized/Denied, source, dual ACK). Empty `[]` is OK.

Native: `python -m platformio run -e native_protocol_tests` then `.pio\build\native_protocol_tests\program.exe` → `native_protocol_tests: ok`. Device image: `python -m platformio run -e heltec_v4_access` succeeded (RAM **9.3%** / flash **19.1%**). Upload to **COM23** was skipped this pass (port not in the live COM list after a 1200 bps reset; mux `acl` not run). Flutter: `flutter test` **15** tests (VIEWER cannot OPEN/LOCK/mutate; OPERATOR OPEN+LOCK TIMEOUT; ADMIN add `CAFEBABE` in fixture). Chrome `http://localhost:8092`: admin login → Доступ → add Ivan/`E18`/`CAFEBABE` + disk photo path (not on the wire) → users 1; OPEN confirm `ACCESS_OPEN` fixture `TIMEOUT`; events UUID present. Five DISCOVER ids unchanged.

Five DISCOVER ids unchanged. `ACCESS_OTA` / AUTO / `COMMAND_EXECUTE` as OPEN still refused.

## Phase 19 — Hardening (docs + audit)

Documentation and verify only. No new ops, adapters, GPIO, Flutter screens, or firmware behaviour.

- Canonical audit: `SECURITY.md` (facts of this image). `docs/security.md` points at it — not a second draft.
- `docs/radio-admin.md`: MeshCore tools (identity / keys / CLI) vs Flutter **Меш** (extender health) vs operator console (no chat).
- Written as product facts (already true in code): GPIO **19/20** USB PHY (`PIN_USB_DM` / `PIN_USB_DP`); photos never on LoRa / OLED / SPIFFS / MESH 485 JSON; fire/security panels keep running if this node dies; dual ACK (`mesh_ok` vs `device_ok` vs `TIMEOUT`); default `BRIDGE_OPEN` = `READ_ONLY`; `RAW_BRIDGE` lab-only and explicit; EM-Marine UID is cloneable convenience ACL (not crypto); CXEM dumps = test vectors.
- Verify: goldens / EventStore / `ACCESS_GET_USERS` have no photo bytes; `pins.h` USB 19/20 unused by Access/RS-485; Flutter `test` still 15 green. Firmware not rebuilt this run.

Five DISCOVER ids unchanged.

## Phase 20 — Local AuthZ (`BAD_AUTH`)

Minimal node-side operator allow-list. Not MeshCore-crypto, not OTA.

- RAM table cap 8 `{name, role}`. Factory lab seed: `viewer` / `operator` / `admin` (same strings the Flutter login chip sends).
- Envelope `operator` is a **name**. Stored role wins. Unknown name, or a keyword that claims a higher role than the row → envelope `BAD_AUTH`.
- `AUTH_SET` `{name, role}` only after the request already resolved as ADMIN. Last ADMIN cannot be dropped this way. Phase 21 persists the table.
- Replay 30/120 unchanged (checked first). VIEWER still cannot OPEN/LOCK/ADD_CARD (`NO_PERMISSION`). Presence on LoRa/BLE/USB is still not authorization.
- Flutter: login chip still picks a factory name. Fixture unknown operator on mutate/OPEN → `BAD_AUTH`.

Native: VIEWER OPEN → `NO_PERMISSION`; unknown `eve` on `ACCESS_OPEN` / `ACCESS_ADD_CARD` / `COMMAND_EXECUTE` → `BAD_AUTH`; factory admin OPEN still dual ACK. Five DISCOVER ids unchanged.

## Phase 21 — Persist operator allow-list

`AUTH_SET` survives reboot. Same blob style as ACL: SPIFFS `/m485_auth.bin` (magic `AUT1` / version 1) + native fake-fs. Not ESP-IDF NVS. Not encrypted.

- `Mesh485Auth::begin()` (from `beginStore()`, like `acl_.begin()`): missing file / bad magic / cap overflow / no ADMIN → factory `viewer` / `operator` / `admin` (logged). Valid file → RAM = file (factory `admin` may be gone; `operator` may be downgraded).
- Write only after a successful `AUTH_SET` (upsert or remove). Not on `NODE_STATUS`. Persist refuses a table with no ADMIN.
- `BAD_AUTH` / replay / VIEWER `NO_PERMISSION` unchanged. Factory names stay public (no password). Presence on the mesh is still not authorization.

Native fake-fs: `AUTH_SET lab=operator` → new dispatcher `begin()` → `lab` OPEN; remove factory `admin` + persist → spoof `admin` is `BAD_AUTH`; corrupt blob → factory. Five DISCOVER ids unchanged.

## Phase 22 — Optional pubkey bind

Allow-list row is `{name, role, pubkey?}`. Empty pubkey = name-only (factory lab unchanged). Bound row: matching **`payload.pubkey`** (64 hex) or `Mesh485Env.sender_pub` → stored role; missing/wrong → `BAD_AUTH`.

- `/m485_auth.bin` write version **2** (32-byte key, zeros = unbound). Version **1** blobs load as unbound — no factory reset.
- `AUTH_SET` `{name, pubkey}` bind; empty/`null` unbind. USB JSON-pubkey is equality, not a MeshCore signature.
- Flutter login chip unchanged. No contacts/chat/QR.

Native: v1 blob loads; bind `lab` → OPEN without key `BAD_AUTH`, matching hex OK, wrong hex `BAD_AUTH`; persist keeps the bind. Five DISCOVER ids unchanged.

Leftovers (not started): AUTO discovery, dual UART, Postgres/FastAPI photo CRUD, OTA, native Orion CONTROL, MeshCore chat/QR.

## Phase 23 — Persist `CONFIG_SET protocol`

`CONFIG_SET protocol` survives reboot. Same blob style as ACL/auth: SPIFFS `/m485_cfg.bin` (magic `CFG1` / version 1, one `protocol` field) + native fake-fs. Not ESP-IDF NVS. Not encrypted. Not baud / parity / AUTO / bridge session.

- Allowed: `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10` | `BOLID_SIGNAL20`. Missing file / bad magic / unknown protocol / `AUTO` in the blob → **`RAW`** (logged).
- `Mesh485BusCfg::begin()` from `beginStore()` (beside `acl_` / `auth_`) then `devices_.setBusProtocol`. Write only after a successful `CONFIG_SET` with `protocol`. Not on `CONFIG_GET` / `NODE_STATUS`. `AUTO` still `UNKNOWN_CMD` / `no AUTO` and does not create or overwrite the file. Empty payload still `persist refused` (baud not in the blob).
- AuthZ 20–22 unchanged (`CONFIG_SET` still ADMIN). Persist does not grant CONTROL on S10/S20 or fill the empty PP allow-list.

Native fake-fs: default `CONFIG_GET` `RAW`; `CONFIG_SET BOLID_SIGNAL10` → new dispatcher `begin()` → `BOLID_SIGNAL10`; same for `BOLID_C2000PP` / `BOLID_SIGNAL20`; corrupt / AUTO-in-file → `RAW`; VIEWER `NO_PERMISSION`; factory admin OPEN / `eve` `BAD_AUTH`. Five DISCOVER ids unchanged.

## Phase 25 — PCN dry contact

GPIO **47** (`INPUT_PULLUP`, active LOW) on the panel node. Debounce 50 ms. A stable edge to LOW writes one EventStore row: `device_id` `PCN-001`, `point_id` `ALARM`, `type` `PCN`, `state` `ALARM`, `severity` `ALARM`, `source` `GPIO`, `device_ok` 1, `mesh_ok` 0. The edge back to HIGH writes one `state` `NORMAL` / `severity` `INFO`. Holding a level does not write again. An open input at start writes nothing. A CRC-valid Orion status frame still does not become `ALARM`.

`DEVICE_DISCOVER` is six ids. The previous five stay. `PCN-001` advertises `READ_STATUS` only. `COMMAND_EXECUTE` on it is `NO_PERMISSION`. OLED READY shows `ТРЕВОГА` or `НОРМА` from the debounced level. Flutter «События» draws `state`. GPIO 19/20 and photos stay out of the JSON. Delivery to the second node is phase 26.

## Phase 26 — PCN hop to the monitor node

A pending `PCN` row on `EXT-001` is a 59-byte frame (`mesh485/pcn_hop.h`): source node, `PCN-001` state, sender sequence, uuid. Not a MeshCore text message. `MeshCoreService::send` floods it as `PAYLOAD_TYPE_RAW_CUSTOM` through the existing `MyMesh` (`AccessMesh` only steals frames with hop magic). `vendor/MeshCore` is unchanged.

`EXT-002` writes the same uuid into its EventStore with `source` `LORA` and answers with an ack of that uuid. A repeat does not add a row. The sender sets `mesh_ok` = 1 only on that ack. No ack: the same uuid is sent again after 1 s, then 2 s, 4 s… The local journal and the GPIO level stay. USB `EVENT_SUBSCRIBE` does not ack a `PCN` row. The hop does not pulse GPIO 40 and does not TX the RS-485 bus. OLED uses the last known PCN state, so the monitor shows `ТРЕВОГА` / `НОРМА` after the frame, not only from its own GPIO 47.

Monitor image: same env with `-D MESH485_NODE_ID=\"EXT-002\"`. Native test is two dispatchers and a byte copy, no radio.

## Phase 27 — RDM6300 on the monitor node

UART1 RX (GPIO **38**, 9600 8N1) parses an RDM6300 frame: `0x02`, 10 hex characters, XOR of those five bytes, `0x03`. The UID is those 10 hex characters and takes the existing `TS-001` ACL path (`acceptCard` / `noteCard`). Empty ACL stays `LOCKED` and writes a `Denied` `RFID` event. A listed card pulses OPEN. The same UID inside 2 s is one row. A bad checksum increments `rdm_err` and is not a card.

The reader is not a new DISCOVER id. It is compile-time `-D MESH485_RDM6300=1` on the monitor image, together with `-D MESH485_NODE_ID=\"EXT-002\"`. In that mode Wiegand 15/16 is not armed and bus adapters do not TX (DE stays RX). The panel image is unchanged. PCN hop still does not open the gate. `setRdmIgnore` is local only; loss of radio does not set it.

## Phase 28 — joint PCN and ACS bench

Same two dispatchers as phase 26, plus one RDM6300 frame on the monitor. An `ALARM` hop leaves the gate `LOCKED`. A UID that is not in the ACL stays `LOCKED` and keeps the alarm on screen. A listed UID pulses OPEN while that alarm is still the shown state. With the ack withheld, the pending `NORMAL` keeps its uuid; the monitor ACL still allows the card; the later ack writes one row. A second panel booted with the contact already closed sends no new hop (phase 25: the first stable level is not an event). The monitor keeps the last delivered state. No seventh DISCOVER id. No air, no live reader, no live panel.

## Remaining order

5. Isolated RS-485 HAL — **done**.
6. MESH 485 protocol (transactions) — **done**.
7. Remote bridge, default READ_ONLY — **done**.
8. DeviceAdapter framework — **done**.
9. Orion research freeze (CRC/timings; CXEM = tests) — **done**.
10. First Bolid adapter read-only (prefer С2000-ПП Modbus) — **done**.
11. Event queue + dual ACK — **done**.
12. Flutter console shell — **done**.
13. Live operator screens + USB/BLE MESH 485 + RU UI — **done**.
14. Allowed CONTROL + capabilities — **done**.
15. Signal-10 adapter (monitor first) — **done**.
16. Signal-20 adapter (monitor first) — **done**.
17. Access ACL + 3MAP — **done**.
18. Employees / cards / OPEN inside the same Flutter app — **done**.
19. Hardening — **done**.
20. Local AuthZ (`BAD_AUTH` + RAM allow-list) — **done**.
21. Persist operator allow-list (`/m485_auth.bin`) — **done**.
22. Optional pubkey bind on allow-list row — **done**.
23. Persist `CONFIG_SET protocol` (`/m485_cfg.bin`) — **done**.
24. С2000-М listen-only MONITOR → EventStore — **queued, separate** (`agent/prompts/24-c2000m-monitor.md`). Orion tap, not the PCN contact.
25. PCN dry contact GPIO 47 → EventStore — **done**. Six DISCOVER ids (`PCN-001` read-only).
26. That event onto the second node — **done**. 59-byte hop, `source` `LORA`, same uuid, ack sets `mesh_ok`. Not chat.
27. RDM6300 on the monitor node into the existing ACL — **done**. Same `TS-001` path. No seventh DISCOVER id.
28. Two-node bench — **done** by fixture (`docs/report-28-ru.md`). Live boards were not run. Next prompt is not assigned.

## Do not start with

Native Orion CONTROL, AUTO discovery, MeshCore Open as the product UI, Excel, OTA, photos-over-mesh, PiDoors PHP, HAPRS ESP-NOW, decrypted Bolid firmware. Cascade leftovers stay leftovers.

## Build checks

```
cd vendor/MeshCore
pio run -e heltec_v4_companion_radio_ble
```

Target firmware env (prompt 02): `heltec_v4_access` with `-D BLE_PIN_CODE=...` **and** `-D ENABLE_USB_INTERFACE`.
