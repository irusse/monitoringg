# Security

Canonical audit: [`SECURITY.md`](../SECURITY.md) (phase 23). This file is **not** a second draft.

That audit is facts of **this image**. It covers:

1. Cloneable EM-Marine / Wiegand UID — convenience ACL, not crypto. Empty store = DENY.
2. BLE NUS + USB CDC pairing (PIN `123456` in the Android system dialog; mux `{` vs Companion).
3. MESH 485 roles from a persisted allow-list (`/m485_auth.bin`, optional pubkey bind, not encrypted), replay 30 s / 120 s, `BAD_AUTH` on unknown / upgraded / mismatched-bound names. USB JSON-pubkey is not a MeshCore signature.
4. GPIO **19/20** = USB PHY; Access / RS-485 maps frozen.
5. Photos never on LoRa / OLED / SPIFFS / MESH 485 JSON.
6. OLED does not show BLE PIN, photos, or MeshCore chat.
7. Fire panel independence; no remote disable-alarm / disarm-all. This node is not a certified fire appliance.
8. Dual ACK: `mesh_ok` vs `device_ok` vs `TIMEOUT`.
9. `RAW_BRIDGE` lab-only; default `BRIDGE_OPEN` = `READ_ONLY`.
10. CXEM dumps = test vectors, not shipped exploits / not firmware source.

Do not invent OTA or operator crypto that is not in the image. Radio vs operator Mesh screen: `docs/radio-admin.md`.
