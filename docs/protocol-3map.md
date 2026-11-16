# 3MAP — 3MESH Access Protocol

Application protocol **on top of MeshCore**. It is not a new mesh.

JSON is for LAN REST/debug. Over LoRa use compact binary (CBOR or packed struct, max payload fits MeshCore packet). JSON examples below are canonical semantics.

## Envelope

```json
{
  "v": 1,
  "type": "access.command",
  "id": "8A12F2",
  "device": "TS-001",
  "cmd": "TURNSTILE_OPEN",
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
  "type": "access.response",
  "id": "8A12F2",
  "device": "TS-001",
  "result": "OK",
  "error": null,
  "ts": 1787912347
}
```

`id` is request id (hex). Replay window: reject same `(id, operator)` within TTL or reject old `ts`.

## Commands

MESH 485 clients send these as `op: ACCESS_*` (3MAP is a **profile namespace**, not a second mesh). OPEN is **not** `COMMAND_EXECUTE`.

| cmd | MESH 485 `op` this run | payload | permission | Status |
|---|---|---|---|---|
| PING | — | | VIEWER | later |
| STATUS | `ACCESS_STATUS` | | VIEWER | **live** |
| TURNSTILE_OPEN | `ACCESS_OPEN` (alias `ACCESS_TURNSTILE_OPEN`) | `{ "pulse_ms": 300 }` | OPERATOR+ | **live** |
| TURNSTILE_LOCK | `ACCESS_LOCK` | | OPERATOR+ | **live** |
| TURNSTILE_UNLOCK | — | | OPERATOR | later (`ACCESS_LOCK` aborts opening) |
| LAMP_ON / LAMP_OFF | — | | OPERATOR | catalog only (`ACCESS_LAMP_*` → `UNKNOWN_CMD`) |
| GET_CONFIG / SET_CONFIG | — | | ADMIN | later |
| GET_USERS | `ACCESS_GET_USERS` | `{ "offset"?, "limit"? }` | ADMIN | **live** (array `{uid, enabled, employee_id?, name?}`, no photos, page 16) |
| SYNC_USERS | — | | ADMIN | later |
| ADD_CARD | `ACCESS_ADD_CARD` | `{ "uid", "employee_id?", "name?", "enabled?" }` | ADMIN | **live** |
| REMOVE_CARD | `ACCESS_REMOVE_CARD` | `{ "uid" }` | ADMIN | **live** |
| UPDATE_CARD | — | | ADMIN | later (ADD_CARD upserts) |
| GET_EVENTS | `ACCESS_GET_EVENTS` | `{ "since": "uuid-or-seq" }` | VIEWER | **live** (same `EventStore` as `EVENT_LIST`, filtered to `TS-001`) |
| SYNC_EVENTS | — | | OPERATOR | later |
| GET_DIAGNOSTICS | — | | VIEWER | use MESH 485 `DIAGNOSTICS` |
| SET_TIME / GET_TIME | — | | ADMIN / VIEWER | later |
| REBOOT / OTA | — | | ADMIN | catalog only (`ACCESS_OTA` / `ACCESS_REBOOT` → `UNKNOWN_CMD`) |

## Errors

`DENIED`, `NO_PERMISSION`, `UNKNOWN_DEVICE`, `BUSY`, `HARDWARE`, `REPLAY`, `BAD_AUTH`, `OFFLINE_QUEUE_FULL`, `UNKNOWN_CMD`

## Events (device → gateway)

Same envelope, `type: "access.event"`.

Each event has a UUID. Sync is idempotent on UUID.

Types: `CARD_ALLOWED`, `CARD_DENIED`, `REMOTE_OPEN`, `REMOTE_LOCK`, `REMOTE_UNLOCK`, `TURNSTILE_OPENED`, `TURNSTILE_CLOSED`, `EMERGENCY`, `DEVICE_ONLINE`, `DEVICE_OFFLINE`, `MESH_ONLINE`, `MESH_OFFLINE`, `CONFIG_CHANGED`, `USER_SYNC`, `OTA_STARTED`, `OTA_COMPLETED`, `ERROR`

Local CSV/log columns (HAPRS-inspired):

```
uuid, timestamp, controller_id, user_id, card_uid, action, result, direction, source
```

Example:

```
2026-08-28 08:31:22  TS-0007  USER-00152  04A79122  ENTRY  ALLOW  RFID
2026-08-28 08:35:10  TS-0007  ADMIN       -         REMOTE_OPEN  ALLOW  MESHCORE
```

## Auth

1. MeshCore identity (packet already encrypted/authenticated by mesh).
2. Local allow-list `{name, role, pubkey?}` (factory `viewer` / `operator` / `admin`; persists as `/m485_auth.bin` v2). Envelope `operator` is a **name**; the stored role wins. Bound row needs matching `payload.pubkey`. Unknown / claimed-up / mismatched key → `BAD_AUTH`. JSON-pubkey on USB is not a MeshCore signature.
3. OPEN is never implied by “can talk on mesh”. VIEWER `ACCESS_OPEN` → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`).

Offline ACL is empty by default: unknown UID = **DENY**. Lab seed: `AccessController::allowUid(hex)` or `-D MESH485_ALLOW_TEST_UID=1` (UID `04A79122`). Do not ship GRANT-all.

## Photos

Never in 3MAP. Server/Flutter only.
