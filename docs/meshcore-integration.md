# MeshCore integration (do not fork)

Status: MESH 485 Extender (ADR 0005). Fill API notes while implementing companion + MESH 485.

## How to build stock Heltec V4 firmware

From `vendor/MeshCore` with PlatformIO:

```text
pio run -e heltec_v4_companion_radio_ble
pio run -e heltec_v4_repeater
pio run -e heltec_v4_companion_radio_usb
pio run -e heltec_v4_companion_radio_wifi
```

Flasher alternative: https://meshcore.io/flasher (companion / repeater / room server).

Windows: CH340 driver if the USB-UART is CH340; V4 native USB CDC may enumerate without it.

DFU: hold BOOT, plug USB or BOOT+RST.

## Board files

| File | Role |
|---|---|
| `variants/heltec_v4/HeltecV4Board.*` | power, FEM, battery |
| `variants/heltec_v4/target.h` | radio, display, button globals |
| `variants/heltec_v4/platformio.ini` | all official envs |
| `boards/heltec_v4.json` | S3 / 16 MB / PSRAM |

## Companion vs repeater

Official MeshCore: **Companion nodes do not repeat**. Repeaters are `examples/simple_repeater`.

3MESH Access needs both:

- Companion interfaces (BLE/USB/Wi-Fi) so Flutter and `meshcore.js` keep working.
- Optional packet forwarding like the repeater, gated by `repeater_enabled`.

Implementation plan:

1. Start from `examples/companion_radio` (keeps Companion Protocol).
2. Import forwarding path from `examples/simple_repeater/MyMesh.cpp` behind a flag.
3. Replace companion OLED (`ui-new` / `ui-tiny`) with Access OLED. Keep MeshCore as a service.
4. Expose `MeshCoreService::setRepeaterEnabled(bool)`.

Do not enable MeshCore's stock chat UI.

## Application API to wrap

Create `firmware/heltec_v4/src/meshcore_adapter/meshcore_service.h`:

```cpp
class MeshCoreService {
public:
  bool start();
  void stop();
  MeshStatus status() const;
  bool send(const uint8_t* data, size_t len, const char* dest);
  bool poll(MeshInbound& out);
  MeshDiagnostics diagnostics() const;
  void setRepeaterEnabled(bool on);
  bool repeaterEnabled() const;
};
```

Call MeshCore from the existing loop/task; do not spawn a second radio driver.

## Host connections (already implemented upstream)

| Firmware define | Path | Client |
|---|---|---|
| `BLE_PIN_CODE` | BLE NUS | meshcore-open, official app |
| `ENABLE_USB_INTERFACE` | USB CDC | meshcore.js serial, Windows |
| `WIFI_SSID` | TCP :5000 | meshcore.js TCP, gateway |

Companion protocol commands (`CMD_APP_START`, `CMD_DEVICE_QUERY`, …) stay untouched. 3MAP rides as application payload (channel message or custom data frame — decide in ADR after reading `companion_protocol.md` and `MyMesh.cpp`).

## Remote repeater management

MeshCore already has remote CLI + `setperm` ACL for infrastructure nodes. Reuse for:

- enable/disable repeater
- diagnostics
- reboot

Turnstile OPEN is **not** a MeshCore CLI string `"open"`. It is a 3MAP command with authz on our side.
