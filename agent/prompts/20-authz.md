PROMPT 20 — AuthZ (первый прогон после каскада 01–19)

Ты продолжаешь **MESH 485 Extender**. Каскад 01–19 **закрыт**. Это **первый пост-каскадный** прогон. Делай **только** его. Не AUTO, не второй UART, не Postgres/фото CRUD, не OTA, не MeshCore chat, не native Orion CONTROL, не GRANT-all, не fire-disable.

Цель: закрыть главный остаток аудита `SECURITY.md` § Residual risk 1 — **роль из строки `operator` можно подделать**. Сейчас `BAD_AUTH` stub: dispatcher **никогда** его не возвращает. Кто пишет NDJSON `{` на USB/BLE, может послать `"operator":"admin"`.

## Продукт (не расширять)

Heltec V4 = MeshCore транспорт + MESH 485 + RS-485 HAL + PROFILE_ACCESS. Пять DISCOVER: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

```
Консоль оператора (без чата)     MeshCore tools (ключи / CLI)
        │                              │
   MESH 485 JSON                    Companion
        │
Heltec  OLED + ACL + EventStore + UART1
        │
   пожарка жива, если меш умер
```

## Заморожено (не пересматривать)

- GPIO **19/20 = USB**. RS-485 39/38/26. Access map `docs/pinout.md`.
- Фото **никогда** на узле / OLED / LoRa / MESH 485 JSON.
- OPEN = `op: ACCESS_OPEN`. LOCK = `ACCESS_LOCK`. Не `COMMAND_EXECUTE`.
- Пустой ACL / неизвестный UID = **DENY**.
- Default `BRIDGE_OPEN` = `READ_ONLY`. `RAW_BRIDGE` только явный.
- `COMMAND_EXECUTE` только `CONTROL_RELAY` на PP-001 именованных `R1`… (allow-list по умолчанию пустой).
- Не TX Bolid кадров «для красоты». Пустая шина = честный `TIMEOUT`.
- Не форкать `vendor/MeshCore`. Не собирать `ui-new` / `DISPLAY_CLASS`.

## Прочитать сначала

- `SECURITY.md` (остаточный риск 1–2, §3 роли / `BAD_AUTH`)
- `docs/cascade-report-ru.md`
- `docs/mesh485-protocol.md` (роли, replay 30/120, таблица ops)
- `firmware/heltec_v4/include/mesh485/roles.h`
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` (`handle`, `opAccess`, `opCommandExecute`)
- Flutter login chip / `operatorName` в `app/mesh485/lib/client/`
- `docs/apps.md`, `docs/radio-admin.md`

## Что сделать в этом прогоне

Минимальный **локальный** AuthZ, не MeshCore-крипто и не OTA.

### Семантика (зафиксировать в коде и в `SECURITY.md` §3)

1. Роль **больше не берётся только из клиента как истина**.
2. На узле — RAM allow-list операторов (cap небольшой, например 8): `{name, role}`.
3. Загрузка:
   - compile-time lab: как минимум `admin` / `operator` / `viewer` с теми же именами, что шлёт Flutter login chip **или**
   - явный `AUTH_SET` **только если** запрос уже прошёл как ADMIN (bootstrap: один factory admin в прошивке).
4. Если `operator` неизвестен или роль в конверте **выше**, чем запись allow-list → envelope `BAD_AUTH` (или `NO_PERMISSION`, но тогда везде одно и то же; предпочтительно **`BAD_AUTH`**, раз токен появился в протоколе).
5. Replay 30/120 **не ломать**.
6. VIEWER по-прежнему не OPEN/LOCK/ADD_CARD. Разница: поддельный `"operator":"admin"` без записи в allow-list больше не проходит.
7. Присутствие на LoRa/BLE/USB по-прежнему **не** авторизация.

Не делать в этом прогоне: MeshCore pubkey table, пароли на OLED, NVS persist allow-list (RAM + документированный factory — достаточно). Persist NVS — отдельный прогон.

### Flutter

- Login chip по-прежнему выбирает роль, но живой клиент шлёт **имя**, которое есть в factory allow-list (`viewer` / `operator` / `admin`).
- Fixture: неизвестный operator → `BAD_AUTH` на mutate/OPEN.
- Не добавлять экраны чата / сотрудников Postgres / OTA.
- Тесты: к текущим 15 добавить кейсы BAD_AUTH (не раздувать UI).

### Документы

- Обновить `SECURITY.md` §3: `BAD_AUTH` больше не stub (описать ровно то, что сделано).
- `docs/mesh485-protocol.md` — строка про `BAD_AUTH`.
- `docs/implementation.md` Phase 20. Коротко в `docs/progress-ru.md` и `docs/cascade-report-ru.md`.
- `AGENTS.md` item 12 / `CASCADE.md`: прогон 20 в таблице, next = этот файл пока не Done.

## Проверки

- `py -3.13 -m platformio run -e native_protocol_tests` затем `program.exe` → `native_protocol_tests: ok`. Не гонять два `heltec_v4_access` параллельно.
- `C:\flutter\bin\flutter.bat test` из `app/mesh485`.
- Native: VIEWER OPEN → `NO_PERMISSION`; неизвестный `operator` на `ACCESS_OPEN` / `ACCESS_ADD_CARD` / `COMMAND_EXECUTE` → `BAD_AUTH`; factory `admin` OPEN по-прежнему dual ACK (на native без GPIO — честный device_ok/HARDWARE как сейчас).
- Опционально, если COM23 свободен: прошить уже собранный `heltec_v4_access`, mux `ACCESS_ADD_CARD` + `ACCESS_GET_USERS` (долг 18/19). Не блокировать прогон отсутствием порта.
- GPIO 19/20 и «фото не в JSON» не менять.

## Запрещено в этом прогоне

- AUTO, dual UART, OTA, Excel, PiDoors PHP, MeshCore chat/QR.
- Фото на SPIFFS / LoRa / OLED / JSON.
- Native Orion CONTROL, fire-disable, GRANT-all.
- Менять карту GPIO Access/RS-485.
- Крипто карт, расписания, группы доступа.
- Выдавать присутствие в меше за авторизацию.
- Писать exploit-рецепты по CXEM.

## Готово когда

1. Поддельный `"operator":"admin"` без factory/allow-list записи получает `BAD_AUTH`.
2. Легитимный factory admin/operator ведёт себя как в фазах 17–18.
3. `SECURITY.md` §3 больше не говорит «BAD_AUTH stubbed / never returned».
4. Native tests ok, Flutter tests зелёные.
5. Пять DISCOVER id без изменений.

Потом **стоп**. Не начинать Postgres, AUTO, OTA, второй UART, Orion CONTROL.
