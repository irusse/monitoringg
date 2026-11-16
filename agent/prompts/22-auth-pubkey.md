PROMPT 22 — Bind optional pubkey to operator allow-list (после 21 persist)

Ты продолжаешь **MESH 485 Extender**. Каскад 01–19 **закрыт**. Фазы **20** (AuthZ) и **21** (persist `/m485_auth.bin`) **сделаны**. Это **третий пост-каскадный** прогон. Делай **только** его. Не AUTO, не второй UART, не Postgres/фото CRUD, не OTA, не MeshCore chat/contacts/QR, не native Orion CONTROL, не GRANT-all, не fire-disable, не смена BLE PIN, не шифрование SPIFFS.

Цель: закрыть остаток `SECURITY.md` Residual risk 1 — **factory имена публичны, пароля нет**. Persist не секрет: кто пишет `{` NDJSON как `admin`, тот ADMIN, пока строка жива. Нужна **опциональная** привязка строки `{name, role}` к pubkey (32 байта / 64 hex). Не полноценная MeshCore-крипто на USB и не мессенджер.

## Продукт (не расширять)

Heltec V4 = MeshCore транспорт + MESH 485 + RS-485 HAL + PROFILE_ACCESS. Пять DISCOVER: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

```
Консоль оператора (без чата)     MeshCore tools (ключи / CLI)
        │                              │
   MESH 485 JSON                    Companion
        │
Heltec  OLED + ACL + EventStore + UART1 + Auth allow-list + optional pubkey
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
- AuthZ 20–21: роль из таблицы, не из клиента; неизвестный / upgraded → `BAD_AUTH`; replay 30/120 до AuthZ; VIEWER не OPEN; persist `/m485_auth.bin`; last ADMIN не удаляется; битый файл → factory.
- Не TX Bolid кадров «для красоты». Пустая шина = честный `TIMEOUT`.
- Не форкать `vendor/MeshCore`. Не собирать `ui-new` / `DISPLAY_CLASS`. Не тащить contacts/chat из `vendor/meshcore-open`.

## Прочитать сначала

- `SECURITY.md` §3 и Residual risk 1 (factory имена; файл не секрет; «No MeshCore pubkey bind»)
- `docs/report-21-ru.md`, `docs/cascade-report-ru.md`, `docs/radio-admin.md`
- `docs/mesh485-protocol.md` (`AUTH_SET`, `BAD_AUTH`)
- `firmware/heltec_v4/include/mesh485/auth.h` + `src/mesh485/auth.cpp` (persist v1 `AUT1`)
- `firmware/heltec_v4/include/mesh485/types.h` + `port.h` (`Mesh485Env.source`)
- `firmware/heltec_v4/src/mesh485/dispatcher.cpp` (`opAuthSet`, `handle` resolve)
- `docs/implementation.md` Phase 21

## Что сделать в этом прогоне

Минимальная **колонка pubkey** в уже существующей таблице. Не пароли, не PIN на OLED, не чат, не подпись всего JSON.

### Семантика

1. Строка allow-list: `{name, role, pubkey?}`. Cap по-прежнему **8**. `pubkey` — 32 байта (64 hex), опционально. Пустой = как сейчас (только имя).
2. Persist: тот же `/m485_auth.bin` + native fake-fs. **Совместимость:** файл фазы 21 (version 1, без pubkey) должен загрузиться как unbound-строки, не factory-сброс. Новый write — version **2** (или запас в `pad`/`reserved`, но честно задокументируй). Не шифровать. По-прежнему нельзя persist без ADMIN.
3. `AUTH_SET` (по-прежнему только после resolve как ADMIN):
   - как сейчас: `{name, role}` upsert; `{name, enabled:false}` remove (не last ADMIN);
   - плюс опционально `{name, pubkey:"<64 hex>"}` привязать; пустая строка / `null` — снять привязку, роль не трогать.
4. Resolve (после replay 30/120, до роли):
   - неизвестное имя / claimed-up → `BAD_AUTH` как в 20;
   - строка **без** pubkey → имя достаточно (factory `admin` / `operator` / `viewer` живут как lab);
   - строка **с** pubkey → нужен совпадающий ключ из конверта (`payload.pubkey` / поле envelope — одно место, задокументируй) **или** `Mesh485Env` sender (если появится). Нет ключа / другой ключ → `BAD_AUTH`.
5. Честно в `SECURITY.md`: pubkey в JSON на USB/BLE **не** доказательство MeshCore-подписи. Это таблица привязки + equality check. Подпись пакета MeshCore / разбор sender из Companion — только если уже есть без форка `vendor/`. Не выдумывать крипто. Не выдавать присутствие в меше за авторизацию.
6. `BAD_AUTH` / replay / VIEWER `NO_PERMISSION` / persist reboot **не ломать**.
7. Factory seed без pubkey (как 21). Не зашивать lab-ключ в прошивку.

Не делать: экран контактов, QR, chat, смена BLE PIN, persist baud/protocol, шифрование ACL/auth файлов, пароли на OLED.

### Flutter

- Login chip без изменений: шлёт factory-имя. Не экран «ключи MeshCore».
- Не добавлять чат / contacts / Postgres / OTA.
- Тесты: не раздувать UI. Живая привязка — native (env/envelope pubkey), не новый виджет. 18 тестов фазы 20–21 зелёные.

### Документы

- `SECURITY.md` §3: optional pubkey bind; Residual risk 1 — unbound factory имена всё ещё публичны; bound строка требует совпадения; JSON-pubkey на USB не подпись.
- `docs/mesh485-protocol.md` — `AUTH_SET` pubkey + правило resolve.
- `docs/implementation.md` Phase 22. Коротко в `docs/progress-ru.md` и `docs/cascade-report-ru.md`.
- Короткий `docs/report-22-ru.md` (или пометка в `docs/report-21-ru.md`).
- `AGENTS.md` item 12 / `CASCADE.md`: прогон 22 в таблице, next = этот файл пока не Done.
- `docs/radio-admin.md`: ключи по-прежнему MeshCore tools; консоль не становится техником радио.

## Проверки

- `py -3.13 -m platformio run -e native_protocol_tests` затем `program.exe` → `native_protocol_tests: ok`. Не гонять два `heltec_v4_access` параллельно.
- `C:\flutter\bin\flutter.bat test` из `app/mesh485` — зелёные (18 тестов не ломать).
- Native:
  - factory admin OPEN по-прежнему dual ACK (unbound);
  - VIEWER OPEN → `NO_PERMISSION`; `eve` → `BAD_AUTH`;
  - persist v1 blob (фаза 21) → load без сброса factory;
  - `AUTH_SET` bind pubkey на `lab` → OPEN без ключа `BAD_AUTH`; с тем же hex — OK; чужой hex — `BAD_AUTH`;
  - remove+persist spoof `admin` по-прежнему `BAD_AUTH`;
  - битый файл → factory.
- Опционально, если COM23 свободен: прошить, mux `ACCESS_ADD_CARD` + `ACCESS_GET_USERS` (долг 18–21). Не блокировать прогон отсутствием порта.
- GPIO 19/20 и «фото не в JSON» не менять.

## Запрещено в этом прогоне

- AUTO, dual UART, OTA, Excel, PiDoors PHP, MeshCore chat/QR/contacts UI.
- Форк `vendor/MeshCore` / правка companion ради «достать ключ любой ценой».
- Фото на SPIFFS / LoRa / OLED / JSON.
- Native Orion CONTROL, fire-disable, GRANT-all.
- Менять карту GPIO Access/RS-485.
- Крипто карт, расписания, группы доступа.
- Persist baud/protocol / шифрование `/m485_acl.bin`.
- Выдавать envelope-pubkey или присутствие в меше за подпись MeshCore.
- Писать exploit-рецепты по CXEM.

## Готово когда

1. Unbound factory имена ведут себя как в фазе 21.
2. Bound строка: нет/чужой pubkey → `BAD_AUTH`; совпадение → роль из таблицы.
3. Файл v1 фазы 21 загружается (строки unbound). Persist по-прежнему требует ADMIN.
4. `SECURITY.md` честно пишет: USB JSON-pubkey ≠ MeshCore signature; factory unbound публичны.
5. Native tests ok, Flutter tests зелёные. Пять DISCOVER id без изменений.

Потом **стоп**. Не начинать chat/QR, Postgres, AUTO, OTA, второй UART, Orion CONTROL.
