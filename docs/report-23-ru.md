# Отчёт — фаза 23 persist CONFIG protocol (11 сентября 2026)

Продукт: **MESH 485 Extender**. Образ `heltec_v4_access`. Каскад **01–19 закрыт**. Фазы **20–22** сделаны. Фаза **23** — четвёртый пост-каскадный прогон, persist `CONFIG_SET protocol`, **сделана**.

Пять DISCOVER без изменений: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

---

## Реализовано в фазе 23

| Что | Как в образе |
|---|---|
| Поле | Только `protocol`: `RAW` \| `BOLID_C2000PP` \| `BOLID_SIGNAL10` \| `BOLID_SIGNAL20`. Default без файла = **`RAW`**. |
| Persist | SPIFFS `/m485_cfg.bin` (magic `CFG1`, version 1, одно поле). Native fake-fs. Не NVS. Не шифрован. Baud / parity / AUTO / сессия моста **не** в блобе. |
| `begin()` | Из `beginStore()`, рядом с `acl_` / `auth_`. Нет файла / битый magic / неизвестный protocol / `AUTO` в файле → **`RAW`** и лог (`mesh485 cfg: … — protocol RAW`). |
| Запись | Только после успешного `CONFIG_SET` с `protocol`. Не на `CONFIG_GET` / `NODE_STATUS`. |
| `AUTO` | По-прежнему `UNKNOWN_CMD` / `no AUTO`. Файл не создаётся и не портится. |
| Baud | По-прежнему отказ (пустой `CONFIG_SET` = `persist refused`). |
| AuthZ | Без изменений. `CONFIG_SET` — ADMIN. VIEWER → `NO_PERMISSION`. Persist не даёт CONTROL на S10/S20 и не открывает пустой PP allow-list. |
| Flutter | Login chip / Settings COM без изменений. Нет экрана шины. |
| Документы | Протокол, implementation Phase 23, `SECURITY.md` residual (`/m485_cfg.bin` не секрет), cascade-report, AGENTS / CASCADE. |

### Проверки фазы 23

- `native_protocol_tests: ok`
- default `CONFIG_GET` → `RAW`; нет файла на GET
- `CONFIG_SET AUTO` → отказ, файл не появляется; после SIGNAL10 AUTO не портит блоб
- empty persist / baud по-прежнему отказ
- `BOLID_SIGNAL10` / `BOLID_C2000PP` / `BOLID_SIGNAL20` переживают simulated reboot (`begin()` + fake-fs)
- битый magic / `AUTO` в блобе / неизвестный protocol / нет файла → `RAW`
- VIEWER `CONFIG_SET` → `NO_PERMISSION`; factory admin OPEN / `eve` `BAD_AUTH`
- пять DISCOVER id без изменений
- Flutter **18** тестов (без нового экрана)
- COM23 в этом прогоне не гоняли

---

## Реализовано раньше (каскад 01–22, кратко)

Access OLED + офлайн ACL, RS-485 UART1 (39/38/26), MESH 485 JSON mux, мост `READ_ONLY`, адаптеры пяти приборов, EventStore + dual ACK, Flutter-консоль без чата, `COMMAND_EXECUTE` только `CONTROL_RELAY` на пустом allow-list PP-001, Сигнал-10/20 MONITOR, фото только с диска, RAM AuthZ + `BAD_AUTH` + persist `/m485_auth.bin` + optional pubkey.

Зафиксировано: GPIO **19/20 = USB**; пустой ACL = DENY; OPEN ≠ `COMMAND_EXECUTE`; присутствие в меше **не** авторизация; пожарка жива, если меш умер.

---

## Не реализовано

### Можно брать следующим прогоном

| # | Задача | Почему не сделано | Риск |
|---|---|---|---|
| 1 | **С2000-М listen-only → EventStore** | Следующий прогон 24. Проводка в `docs/c2000m.md`. | Не чат MeshCore; не второй мастер на шине. |
| 2 | Живой mux ACL на COM23 | Порта не было в 18–23. | Не доказано: `ACCESS_GET_USERS` после `ACCESS_ADD_CARD`; persist protocol после ребута узла. |
| 3 | Физический свайп Wiegand | Стенд не собирали. | GRANT/DENY на реальном считывателе не доказан. |
| 4 | Подпись MESH 485 пакетов MeshCore | Нет sender в mux без форка `vendor/`. | JSON-`pubkey` на USB по-прежнему подделывается. Factory unbound публичны. |

### Не стартовать без отдельного решения

| Задача | Статус |
|---|---|
| AUTO discovery | `UNKNOWN_CMD`. Протокол шины только вручную. |
| Persist baud | По-прежнему отказ (тест 15). Не в блобе protocol. |
| Второй RS-485 / dual UART | Нет. Нужен расширитель. **Не** GPIO 19/20. |
| Postgres + FastAPI CRUD / `PHOTO_DIR` | `/employees` = `{items:[]}`. Фото не в LoRa. |
| OTA / `ACCESS_OTA` | `UNKNOWN_CMD`. Нет подписи прошивки. |
| Native Orion CONTROL (реле Сигнал-10/20) | Нет. CRC ≠ исполнение. |
| `BRIDGE_OPEN` кнопкой оператора во Flutter | Placeholder READ_ONLY. |
| Excel / отчёты | На сервере, не на ESP32. |
| MeshCore чат / контакты / QR | Запрещено. |
| Расписания / группы / праздники | ACL — плоский список UID, cap 64. |
| Крипто EM-Marine / anti-clone | UID клонируемый. |
| Смена BLE PIN `123456` | Lab default. |
| Шифрование SPIFFS blobs | Нет crypto. |
| GRANT-all | Намеренно нет. |
| Сертификация пожарки | Не претендуем. |
| PiDoors PHP / MariaDB | Не брать. |

---

## Следующий прогон

Один файл: [`agent/prompts/24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md).

Цель: С2000-М RS-485 listen-only → EventStore (не MeshCore DM, не второй мастер). Проводка: [`c2000m.md`](c2000m.md).
