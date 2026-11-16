# hass-hubc2000pp entity names (model only)

Extracted 2026-08-28 from `vendor/hass-hubc2000pp` (Apache-2.0, [Regressor/hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp)). **Names and mapping only.** Do not copy the Home Assistant component, `script.js`, or the TCP/UDP stack into ESP32 firmware. That repo talks to **HUB-C2000PP over IP**, not Orion UART.

Full numbered event/status dictionaries stay in the clone: `custom_components/hubc2000pp/const.py`. This file is the MESH 485 mapping sketch.

## Transport (server-side later)

| Path | Port | Role |
|---|---|---|
| UDP PING / PONG | host **TCP/UDP 22000** (HA default `port`) | Integration startup: send `PING`, expect `PONG`. Not ESP32. |
| Poll | same 22000 | Once per **minute**: `getZones`, `getParts`, `getRelays`. |
| UDP push | **port + 1** (22001 if 22000) | `zone:<uid>:<state>`, `part:<id>:<state>`, `relay:<id>:<state>`. ADC is **not** pushed. |

HA platforms: `sensor`, `binary_sensor`, `alarm_control_panel`, `switch`.

## Zone (ШС)

HUB line: `zone:<db_id>:<sh>:<part>:<stype>:<state>:<adc>:<sensorType>:<dev>:<desc>`

| Field | Meaning |
|---|---|
| `id` / `db_id` | Zone row in С2000-ПП DB (1..512 on the converter) |
| `uid` | `{id}.{sh}.{part}.{stype}` |
| `sh` | Loop number on the Orion device |
| `part` | Partition (section) id |
| `stype` | Zone type code from the converter |
| `state` | Numeric event/status code (see families below) |
| `adc` | Analog (poll only) |
| `desc` | Operator name (no colons in names — HA parser split) |
| `dev` | Device address / name from HUB |

Typical HA `type` strings from `script.js` `sensor_types` (site-specific map, not a Bolid enum):

`doorSensor`, `motionSensor`, `smokeSensor`, `windowSensor`, `temperatureSensor`, `humiditySensor`, `statusSensor`, `ripOutputSensor`, `ripCurrentSensor`, `ripBatteryVoltageSensor`, `ripBatteryLevelSensor`, `ripInputVoltageSensor`, `soundSensor`, `carbonMonoxideSensor`, `counterSensor`, `counterTotalSensor`, `counterTotalIncSensor`, `genericAdcSensor`, `unknownSensor`.

Binary “on” uses event-code sets: smoke → `FIRE_EVENTS`; door/window/motion → `DOOR_EVENTS` / `MOTION_EVENTS`.

**MESH 485:** `Point.type` = `zone` (ШС) or `loop` when the point is the physical loop on a panel. `Event.type` from the code family (table below).

## Relay

HUB: `relay:<id>:<stat>:<desc>` — HA `switch`, model `Relay`, unique id `relay_{id}`. `stat == "true"` → on.

Converter DB: up to **255** relays. **MESH 485:** `Point.type` = `relay`. State `ON` / `OFF`. Prompt 10 is read-only: no `relay_on` / `relay_off` (those exist only on the HA/HUB IP API).

## Section (partition / раздел)

HUB: `part:<id>:<stat>:<desc>` — HA `alarm_control_panel`, model `Раздел`, unique id `partition_{id}`, name `Раздел {id}` if desc empty. Converter DB: up to **64** sections.

HA maps `stat` (numeric code) via:

| HA alarm state | Code sets in const.py |
|---|---|
| `armed_away` | `ARMED_EVENTS` (e.g. 24 взят, 1 сеть 220 в норме, …) |
| `arming` | `ARMING_EVENTS` (23 идёт взятие) |
| `disarmed` | `DISARMED_EVENTS` (109 снят, 112 контроль выкл, 117/119 снят и …) |
| `triggered` | anything else (including fire/alarm codes) |

HUB IP also has `arm:` / `disarm:` — **not** for ESP32 this cascade; CONTROL is prompt 14.

**MESH 485:** `Point.type` = `section`.

## Event types / severity-like labels

`DEVICE_EVENTS_DICT` is the operator string for a numeric code (0..255, sparse). `DEVICE_STATUSES_DICT` is the subset used as **steady state** labels (Норма / Взят / Снят / …).

Families used by the HA component (codes, not Python):

| Family | Codes | Typical MESH `Event.type` | `severity` |
|---|---|---|---|
| Fire-ish | 19, 37, 40, 44, 76 | `FIRE` | `ALARM` |
| Intrusion / door | 3, 15, 18, 27, 119, 149, 216 | `ALARM` / `TAMPER` / `DOOR` | `ALARM` |
| Alarm set | 3, 18, 27, 33, 37, 40, 44, 45, 58, 79, 137–139, 141, 144–147, 149, 161, 214, 229, 230, 250, 252 | `ALARM` | `ALARM` |
| Armed / restore | 1, 24, 35, 39, 47, 72, 78, 80, 83, 91, 123, 188, 191, 195, 199, 218 | `RESTORE` / `ARMED` | `INFO` |
| Arming | 23 | `ARMING` | `INFO` |
| Disarmed | 109, 112, 117, 119 | `DISARMED` | `INFO` |
| Link | 250 потеря связи, 251 восстановление | `FAULT` / `RESTORE` | `WARN` / `INFO` |

Example names (not exhaustive): 24 «Взятие ШС на охрану», 37 «Пожар», 109 «Снятие ШС с охраны», 128 «Изменение состояния выхода (реле)», 241 «Раздел взят», 242 «Раздел снят».

## Mapping sketch (HA → MESH 485)

| HA / HUB | MESH 485 `Point.type` | MESH 485 `Event.type` (sketch) |
|---|---|---|
| zone binary (smoke) | `zone` | `FIRE` |
| zone binary (door/motion) | `zone` | `ALARM` |
| zone ADC sensor | `zone` | `ADC` / none (poll) |
| `statusSensor` | `zone` or `loop` | from code family |
| relay switch | `relay` | `RELAY` |
| partition alarm panel | `section` | `ARMED` / `DISARMED` / `ALARM` |

Clients still see Device / Point / Event, never “Bolid command 0x17” and never HUB `getZones` strings on the mesh.
