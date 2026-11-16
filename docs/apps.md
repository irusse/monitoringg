# Applications — Android and Windows (same UI)

One Flutter console for **MESH 485 Extender**. Spec: `docs/apps.md` plus `docs/oled-ui.md` on the node.

Not a MeshCore messenger. Companion codec from `vendor/meshcore-open` **library only**. Operator chrome is **Russian** (`flutter gen-l10n`, default locale `ru`). Protocol ids stay Latin (`RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`, `EXT-001`).

## Screens (both platforms)

| Screen | Job |
|---|---|
| Login | role: наблюдатель / оператор / администратор |
| Dashboard | objects, devices, NORMAL/FAULT/ALARM counts, extender online |
| Objects | site card: C2000-ПП + Signal-10 + Signal-20 + turnstile + RAW |
| Devices | Device/Point list; capabilities include `CONTROL_RELAY` only when advertised |
| Events | live log from `EVENT_LIST` / USB `EVENT_SUBSCRIBE`; dual ACK |
| Control | capability buttons + extra confirmation; mesh ACK vs device ACK vs timeout |
| Bridge | READ_ONLY / off placeholder; shows `NODE_STATUS.session`; no operator `BRIDGE_OPEN` |
| Mesh | **extender nodes** only: RSSI, SNR, RX/TX, uptime, last_seen — not chat |
| Access | `TS-001`: employees/cards (ADMIN `ACCESS_GET_USERS` / `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD`), photos **from disk only**, OPERATOR+ OPEN+LOCK (`ACCESS_OPEN` / `ACCESS_LOCK`, confirm), live TS-001 events |
| Diagnostics | UART/CRC/timeouts, mesh vs device ACK (folded into Mesh/Events this run) |
| Settings | COM / BLE picker, role chip, transport label |

**Forbidden:** MeshCore contacts, channels, chat, QR, photos over radio.

## Status (Phase 20)

`app/mesh485` talks live MESH 485 NDJSON. Windows primary = USB CDC (bench **COM23** if present). Opening CDC often resets the S3 — client waits ~2.5 s and skips non-`{` lines. Android = BLE NUS to `3MESH-TS-001`. No port / timeout → **fixture** JSON; Settings shows the active transport.

Roles: login chip still picks `viewer` / `operator` / `admin` (factory allow-list names). Unknown name on mutate/OPEN → `BAD_AUTH`. VIEWER ops unchanged. `COMMAND_EXECUTE` is OPERATOR+ and only for advertised capabilities. Control screen confirms then shows `mesh_ok` / `device_ok` / `TIMEOUT`. Firmware default PP-001 allow-list is empty (no `CONTROL_RELAY`). Fixture advertises `CONTROL_RELAY` + `R1` for UI tests (empty-bus result = TIMEOUT). Forbidden: disable-fire-alarm / disarm-all. Access: OPERATOR+ OPEN/LOCK (`ACCESS_OPEN` / `ACCESS_LOCK`, confirm). ADMIN lists/adds/disables cards via `ACCESS_GET_USERS` / `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD` (node ACL). Photos are local files in the Flutter UI only — never in MESH 485 JSON, SPIFFS, OLED, or LoRa. FastAPI `/employees` is still `{items:[]}`; node-only employees until Postgres CRUD. Fixture OPEN/LOCK ACK is honest TIMEOUT (no GPIO); ADMIN add-card succeeds in fixture RAM. `BRIDGE_OPEN` is still not an operator action.

Heltec OLED strings remain Russian (`docs/oled-ui.md`). Hardening audit: `SECURITY.md`. Radio vs Mesh screen: `docs/radio-admin.md`.

## Transports

| Transport | Android | Windows | Payload |
|---|---|---|---|
| BLE NUS (muxed `{`) | primary | if BT works | MESH 485 NDJSON |
| USB CDC (muxed `{`) | OTG optional | **primary** | same |
| Fixture JSON | fallback | fallback | same shape, in-memory |
| LAN REST | if server | if server | photos, bulk, reports |

Repeater **identity** still uses MeshCore tools when needed. The Mesh screen is gateway health, not a radio technician shell (`docs/radio-admin.md`).
