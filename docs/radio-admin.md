# Radio / repeater admin vs operator Mesh

Two jobs, two tools. Do not use the Flutter console as a MeshCore technician shell, and do not use MeshCore chat apps as the MESH 485 operator console.

Product UI: `docs/apps.md`. Node OLED: `docs/oled-ui.md`. Pins: `docs/pinout.md`. Audit: `SECURITY.md`.

## Which tool

| Job | Tool | Transport |
|---|---|---|
| Repeater identity, keys, MeshCore CLI, companion radio setup | **MeshCore companion / official tools** (`vendor/MeshCore`, meshcore-cli, MeshCore client). Not the Flutter **Меш** tab. | USB CDC Companion frames **or** BLE NUS (name `3MESH-TS-001`, PIN `123456` in the **system** pairing dialog) |
| Extender health (RSSI, SNR, RX/TX, uptime, last_seen, `session`) | Flutter **Меш** — MESH 485 `NODE_STATUS` / `DIAGNOSTICS` | USB or BLE mux: lines starting with `{` |
| Operator objects, devices, events, Control, Access, Bridge status | Flutter console — **no** contacts, channels, or chat | Same MESH 485 NDJSON |
| Local repeater on/off on this Heltec | OLED USER menu (`ПОВТОР ВКЛ` / `ВЫКЛ`) — persists on the node | GPIO 0; identity/keys still MeshCore tools |

## MeshCore tools (technician)

Use these when you need **radio identity**, not site objects:

- Flash / companion CLI over USB-C.
- BLE pairing to the MeshCore companion (PIN is Android system UI, never the SSD1306).
- Repeater path, keys, and MeshCore packet-level diagnostics.

Those tools speak Companion binary (first byte is a command id, never `{`). The firmware USB/BLE mux forwards non-`{` bytes to MeshCore. Do not paste MESH 485 JSON into a Companion-only client unless it muxes `{` the same way.

The Flutter app extracts a companion **codec** from `vendor/meshcore-open` **library only**. It does not embed MeshCore Open chat, contacts, channels, or QR.

## Flutter Меш (operator)

The **Меш** screen is **extender node health** for this gateway (`EXT-001` / OLED `TS-001`):

- RSSI, SNR, RX/TX counters, uptime, last_seen
- Bridge session hint (`NODE_STATUS.session`)
- Dual ACK leftovers live on Events / Diagnostics (`mesh_ok` vs `device_ok`)

It is **not**:

- A contact list or channel list
- A MeshCore messenger
- A place to set radio keys
- A photo or employee store (that is Access, disk-only — `docs/apps.md`)

## Operator console (everything else)

One Flutter app (`app/mesh485`), Android = Windows:

- Dashboard / Objects / Devices / Events / Control / Bridge (READ_ONLY placeholder) / Access / Settings
- Roles: наблюдатель / оператор / администратор → envelope names `viewer` / `operator` / `admin` (factory allow-list, optional pubkey bind via `AUTH_SET`). Unknown name → `BAD_AUTH`. Radio keys stay MeshCore tools — not this console.
- OPEN/LOCK = `ACCESS_OPEN` / `ACCESS_LOCK`, not `COMMAND_EXECUTE`
- Photos stay on phone/PC disk (`PhotoStore` path). Never LoRa / OLED / MESH 485 JSON.

Presence on the mesh is not authorization (`SECURITY.md` §3).

## USB / BLE on the bench

Windows primary = USB CDC (this bench: **COM23** when present). Re-opening CDC often resets the ESP32-S3 — wait ~2.5 s and skip non-`{` lines.

```
cd firmware/heltec_v4
python scripts/m485_devices.py COM23 all
```

Android primary = BLE NUS to `3MESH-TS-001`. Same mux: `{` → MESH 485.

GPIO **19/20** are the USB PHY. Do not move RS-485 or Access I/O onto them.

## Fire / radio loss

If LoRa, BLE, USB, or the app die, С2000 / Signal-10 / Signal-20 on the isolated bus **keep running**. This node is a gateway, not the panel. Default bridge privilege remains `READ_ONLY` (`docs/bridge.md`).

С2000-М fire/security events are **MESH 485** (`EVENT_LIST` / `m485.event` / Flutter Events), including when a second extender should see the same journal over LoRa. They are **not** MeshCore private chat / Companion DM. Wiring: `docs/c2000m.md`.
