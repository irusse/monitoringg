# Heltec V4 Access firmware

Hybrid MESH 485 Extender + Access profile (MeshCore companion BLE **and** USB). Product OLED owns the panel.

## Do not

- Edit `vendor/MeshCore` core.
- Show MeshCore chat UI on OLED (`ui-new` is not compiled; no `DISPLAY_CLASS`).
- Bit-bang a new Wiegand decoder (use WiegandNG).
- Use GPIO 19/20 for Access I/O (USB PHY).

## Build / flash (Phase 02)

Heltec V4 on USB-C (this workspace: **COM23**):

```
cd firmware/heltec_v4
pio run -e heltec_v4_access
pio run -e heltec_v4_access -t upload
pio device monitor -e heltec_v4_access
```

Native USB (VID `303A:1001`) on this board can drop a long single `write_flash` around 20 s. If `pio run -t upload` fails near 90%, flash in 256 KB chunks (bootloader + app pieces) at 460800; `--after hard_reset` on the last chunk.

If `pio run` fails in SCons with `TypeIs` from `typing`, use Python 3.12 on PATH.

After reset the USB serial must print:

```
3MESH ACCESS
BLE name: 3MESH-TS-001
BLE pin: 123456
USB CDC ready
```

SSD1306 shows Access **READY** (`TS-001`, `Present card`, `PP-001 FAULT` until a CRC-valid С2000-ПП read, or `S10-001 FAULT` when CONFIG protocol is `BOLID_SIGNAL10`, or `S20-001 FAULT` when it is `BOLID_SIGNAL20`, `M485 9600` when UART1 is up, `M●` when radio init succeeds, `R` if repeater on). Not MeshCore splash/chat. After 10 s idle the panel is DISPLAYOFF; USER wakes it (that press is not a command).

- Short press **USER**: demo GRANTED → DENIED → REMOTE OPEN → LOCKED → EMERGENCY → READY.
- Long press **USER** (>1.5 s): **MENU** — Info / Repeater ON|OFF / Back. Long-press again to select (repeater toggle is local).
- MESH 485 JSON on USB: first byte `{` is muxed (not Companion). `python scripts/m485_node_status.py COM23` sends `NODE_STATUS`. Devices: `python scripts/m485_devices.py COM23 discover` then `info RAW-001`. Bridge: `python scripts/m485_bridge.py COM23 open|poll|close` (`READ_ONLY` default; `txn` is `NO_PERMISSION` while the session is open). Node id `EXT-001`. Companion BLE+USB stays on the same image.
- RS-485: UART1 TX=39 RX=38 DE=26, 9600 8N1. **No bus echo.** Idle RX updates OLED `RS485 N B` (or `BR RO` during a session). TX via `RS485_TRANSACTION` in MONITOR, or during an explicit `RAW_BRIDGE` session. `READ_ONLY` session captures RX into a ring drained by `BRIDGE_POLL`. Isolated A/B transceiver required on a real bus. `DEVICE_DISCOVER` lists `RAW-001`, `TS-001`, `PP-001` (`BOLID_C2000PP`), `S10-001` (`BOLID_SIGNAL10`, MONITOR), and `S20-001` (`BOLID_SIGNAL20`, MONITOR). Empty A/B: Bolid adapters `online: false` (TIMEOUT), `EVENT_LIST` `[]` — not a fake fire event. Bus protocol is manual `CONFIG_SET` (`RAW` / `BOLID_C2000PP` / `BOLID_SIGNAL10` / `BOLID_SIGNAL20`; **no AUTO**).
- OLED: long-press USER on the `BRIDGE` screen aborts the session (DE RX, back to READY). Panel stays awake while a session is open.
- Repeater: `MeshCoreService::setRepeaterEnabled(bool)`, default **on** (first Access boot), persisted in SPIFFS. Companion `allowPacketForward()` follows this flag.
- BLE NUS name `3MESH-TS-001`, PIN `123456`. USB CDC is MeshCore companion (`MultiSerialInterface`) after the banner; JSON lines are intercepted by the mux.

Compile-only without radio (CI):

```
pio run -e heltec_v4_access_stub
```

Native MESH 485 tests (no Arduino HAL):

```
pio run -e native_protocol_tests
.pio/build/native_protocol_tests/program.exe
```

Orion CRC-8 is unit-tested in that env (`crc8_orion`) and used by the Signal-10 and Signal-20 adapters only. It is **not** on `RS485_TRANSACTION`. Modbus RTU CRC-16 is used only by the С2000-ПП adapter.

On Windows, if MinGW `g++` is missing, `extra_native_msvc.py` shims `g++` to Visual Studio `cl.exe`.

### MESH 485 Device / Point / Event (USB mux)

After boot, CDC is MeshCore Companion except lines that start with `{`. Compact NDJSON + newline:

```
python scripts/m485_devices.py COM23 status
python scripts/m485_devices.py COM23 discover
python scripts/m485_devices.py COM23 info RAW-001
python scripts/m485_devices.py COM23 points
python scripts/m485_devices.py COM23 events
python scripts/m485_devices.py COM23 all
```

Expect `RAW-001` (`RAW_RS485`), `TS-001` (`ACCESS`), `PP-001` (`BOLID_C2000PP`, read-only; empty bus `online: false` / TIMEOUT), `S10-001` (`BOLID_SIGNAL10`, MONITOR; TX only after `CONFIG_SET` protocol `BOLID_SIGNAL10`), and `S20-001` (`BOLID_SIGNAL20`, MONITOR; TX only after `CONFIG_SET` protocol `BOLID_SIGNAL20`). `COMMAND_EXECUTE` is `NO_PERMISSION` unless PP-001 advertises `CONTROL_RELAY`. Empty bus: `EVENT_LIST` may be `"events":[]` — do not treat that as a missing adapter.

Stock MeshCore images (separate, not this Access UI):

```
cd vendor/MeshCore
pio run -e heltec_v4_companion_radio_ble
pio run -e heltec_v4_repeater
```

## Layout

```
src/main.cpp                 boot + cooperative loop
src/meshcore_adapter/        MeshCoreService (companion BLE+USB)
src/hal/rs485/               isolated UART + DE/RE (prompt 05)
src/mesh485/                 MESH 485 protocol, bridge, sync
src/protocols/               DeviceAdapter implementations (Bolid, Modbus, RAW)
src/device/                  registry, Device/Point/Event
src/access/                  ACL + Access profile
src/reader/                  Wiegand
src/turnstile/               relay, lamp, sensors
src/storage/                 LittleFS users/events
src/display/                 product OLED (SSD1306)
src/network/                 Wi-Fi REST/OTA (later)
src/diagnostics/
include/access/pins.h        frozen GPIO map (Access + RS-485)
```

## Pin defaults

See `docs/pinout.md` and `hardware/wiring.md`. OLED: SDA 17 / SCL 18 / RST 21, VEXT 36 **LOW = on**. Wiegand D0/D1 15/16. OPEN 40, lamps 41/33, ENTRY/EXIT/ESTOP 3/4/6. RS-485 UART1 TX/RX/DE = 39/38/26.
