# PROMPT 02 — Firmware BLE + USB

Heltec V4: MeshCore companion with **both** `BLE_PIN_CODE` and `ENABLE_USB_INTERFACE` (`MultiSerialInterface`).

**Keep the Access OLED from run 1.** Do not restore MeshCore `ui-new` / repeater splash as the main screen.

- `setRepeaterEnabled(bool)`, default on, disableable
- Env `heltec_v4_access` using MeshCore `heltec_v4_oled` flags
- Serial: `3MESH ACCESS`, BLE name, USB CDC ready
- Indicator `M●` must reflect real radio status when MeshCore starts

No Wiegand ACL yet. Update `firmware/heltec_v4/README.md`.
