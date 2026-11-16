PROMPT 21 — Persist operator allow-list (после 20 AuthZ)

Ты продолжаешь **MESH 485 Extender**. Каскад 01–19 **закрыт**. Фаза **20** (RAM AuthZ + `BAD_AUTH`) **сделана**. Это **второй пост-каскадный** прогон. Делай **только** его. Не AUTO, не второй UART, не Postgres/фото CRUD, не OTA, не MeshCore chat, не MeshCore pubkey table, не native Orion CONTROL, не GRANT-all, не fire-disable.

Цель: закрыть остаток фазы 20 — **allow-list операторов только в RAM**. Сейчас `AUTH_SET` и смена ролей живут до ребута; после ребута снова factory `viewer` / `operator` / `admin`. Кто один раз прошёл как ADMIN и сузил список — теряет это при сбросе питания.

## Продукт (не расширять)

Heltec V4 = MeshCore транспорт + MESH 485 + RS-485 HAL + PROFILE_ACCESS. Пять DISCOVER: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

```
Консоль оператора (без чата)     MeshCore tools (ключи / CLI)
        │                              │
   MESH 485 JSON                    Companion
        │
Heltec  OLED + ACL + EventStore + UART1 + Auth allow-list
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
- AuthZ фазы 20: роль из RAM-таблицы, не из клиента; неизвестный / upgraded → `BAD_AUTH`; replay 30/120 до AuthZ; VIEWER не OPEN.
- Не TX Bolid кадров «для красоты». Пустая шина = честный `TIMEOUT`.
- Не форкать `vendor/MeshCore`. Не собирать `ui-new` / `DISPLAY_CLASS`.

## Прочитать сначала

- `SECURITY.md` §3 и Residual risk 1 (factory имена публичны; нет persist)
- `docs/report-20-ru.md`, `docs/cascade-report-ru.md`
- `docs/mesh485-protocol.md` (`AUTH_SET`, `BAD_AUTH`)
- `firmware/heltec_v4/include/mesh485/auth.h` + `src/mesh485/auth.cpp`
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` (`opAuthSet`, `handle` resolve)
- Persist-образец ACL: `access/access_controller` + `/m485_acl.bin` + `access_acl_use_fake_fs` (native reboot)
- `docs/implementation.md` Phase 20

## Что сделать в этом прогоне

Минимальный **persist** allow-list. Не пароли, не pubkey, не смена BLE PIN.

### Семантика

1. Таблица `{name, role}` (cap 8) **переживает ребут**.
2. Хранение: тот же стиль, что ACL — файл на SPIFFS (например `/m485_auth.bin`) + native fake-fs. Не обязательно ESP-IDF `nvs_*`; blob + magic/version достаточно. Не шифровать.
3. Загрузка в `begin()` (вызвать из dispatcher, как `acl_.begin()`):
   - нет файла / битый magic / cap overflow → **factory** `viewer` / `operator` / `admin` и честный лог/комментарий, не молчаливый мусор;
   - валидный файл → RAM = файл (даже если factory `admin` снят или `operator` понижен).
4. Запись: после успешного `AUTH_SET` (upsert и remove). Не писать на каждый `NODE_STATUS`.
5. Нельзя persist-нуть таблицу **без** хотя бы одного ADMIN (как в 20: last admin не удаляется через `AUTH_SET`). Битый файл без ADMIN → factory, не «все VIEWER».
6. `BAD_AUTH` / replay / VIEWER `NO_PERMISSION` **не ломать**.
7. Factory имена по-прежнему публичны (нет пароля). Persist не делает присутствие в меше авторизацией.

Не делать: MeshCore pubkey, пароли на OLED, persist `CONFIG_SET` baud/protocol, шифрование ACL карт, смена BLE PIN.

### Flutter

- Login chip без изменений: шлёт `viewer` / `operator` / `admin`.
- Не добавлять экран управления операторами / чат / Postgres / OTA.
- Тесты: не раздувать UI. Если нужен кейс — unit/fixture «после AUTH_SET имя остаётся» только если это не требует нового экрана. Живой persist проверяется native fake-fs reboot, не Flutter.

### Документы

- `SECURITY.md` §3: список **persist** (описать ровно файл/blob, без NVS-фантазий). Residual risk 1: factory имена всё ещё публичны; файл не шифрован.
- `docs/mesh485-protocol.md` — строка: allow-list переживает ребут.
- `docs/implementation.md` Phase 21. Коротко в `docs/progress-ru.md` и `docs/cascade-report-ru.md`.
- `docs/report-20-ru.md` — пометка, что persist закрыт (или короткий `docs/report-21-ru.md`).
- `AGENTS.md` item 12 / `CASCADE.md`: прогон 21 в таблице, next = этот файл пока не Done.

## Проверки

- `py -3.13 -m platformio run -e native_protocol_tests` затем `program.exe` → `native_protocol_tests: ok`. Не гонять два `heltec_v4_access` параллельно.
- `C:\flutter\bin\flutter.bat test` из `app/mesh485` — зелёные (не ломать 18 тестов фазы 20).
- Native:
  - factory admin OPEN по-прежнему dual ACK;
  - VIEWER OPEN → `NO_PERMISSION`; неизвестный `eve` → `BAD_AUTH`;
  - `AUTH_SET` lab=operator → persist; новый dispatcher + fake-fs `begin()` → `lab` OPEN проходит, spoof `admin` после remove+persist → `BAD_AUTH`;
  - битый файл → factory, не пустая таблица.
- Опционально, если COM23 свободен: прошить `heltec_v4_access`, `AUTH_SET` + ребут + тот же `operator` жив; mux `ACCESS_ADD_CARD` + `ACCESS_GET_USERS` (долг 18–20). Не блокировать прогон отсутствием порта.
- GPIO 19/20 и «фото не в JSON» не менять.

## Запрещено в этом прогоне

- AUTO, dual UART, OTA, Excel, PiDoors PHP, MeshCore chat/QR, pubkey table.
- Фото на SPIFFS / LoRa / OLED / JSON.
- Native Orion CONTROL, fire-disable, GRANT-all.
- Менять карту GPIO Access/RS-485.
- Крипто карт, расписания, группы доступа.
- Persist baud/protocol / шифрование `/m485_acl.bin`.
- Выдавать присутствие в меше за авторизацию.
- Писать exploit-рецепты по CXEM.

## Готово когда

1. `AUTH_SET` переживает simulated reboot (native fake-fs) и не теряется.
2. Битый / отсутствующий файл → factory, всегда есть ADMIN.
3. `BAD_AUTH` / replay / VIEWER OPEN как в фазе 20.
4. `SECURITY.md` §3 говорит, что список persist (и что файл не секрет).
5. Native tests ok, Flutter tests зелёные. Пять DISCOVER id без изменений.

Потом **стоп**. Не начинать pubkey, Postgres, AUTO, OTA, второй UART, Orion CONTROL.
