# Отчёт — фаза 21 persist allow-list (30 августа 2026)

Продукт: **MESH 485 Extender**. Образ `heltec_v4_access`. Каскад **01–19 закрыт**. Фаза **20** AuthZ сделана. Фаза **21** — второй пост-каскадный прогон, persist списка операторов, **сделана**.

Пять DISCOVER без изменений: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

---

## Реализовано в фазе 21

| Что | Как в образе |
|---|---|
| Список переживает ребут | `{name, role}` cap 8 в RAM + SPIFFS `/m485_auth.bin` |
| Формат | Тот же стиль, что ACL: packed header (magic `AUT1` = `0x41555431`, version 1, count, cap) + 8 записей. Не ESP-IDF `nvs_*`. Не шифрован. |
| Native reboot | `mesh485_auth_use_fake_fs` / `mesh485_auth_reset_fake_fs` / `mesh485_auth_poke_fake_fs` — как ACL. |
| `begin()` | Из `beginStore()`, как `acl_.begin()`. Нет файла / битый magic / cap overflow / нет ADMIN → factory `viewer` / `operator` / `admin` и лог (`mesh485 auth: … — factory viewer/operator/admin`), не молчаливый мусор. Валидный файл → RAM = файл (factory `admin` может быть снят). |
| Запись | Только после успешного `AUTH_SET` (upsert и remove). Не на `NODE_STATUS`. Persist отказывается писать таблицу без ADMIN. |
| AuthZ фазы 20 | `BAD_AUTH` / replay 30/120 / VIEWER `NO_PERMISSION` не сломаны. |
| Flutter | Login chip без изменений. Нет экрана управления операторами. Persist проверяется native fake-fs, не UI. |
| Документы | `SECURITY.md` §3: список persist, файл не секрет. Протокол, implementation Phase 21, cascade-report, AGENTS / CASCADE. |

### Проверки фазы 21

- `native_protocol_tests: ok`
- factory admin OPEN — dual ACK; VIEWER OPEN → `NO_PERMISSION`; `eve` → `BAD_AUTH`
- `AUTH_SET lab=operator` + новый dispatcher `begin()` → `lab` OPEN
- remove factory `admin` + persist → spoof `admin` → `BAD_AUTH`
- битый файл / нет ADMIN в блобе / overflow / нет файла → factory, всегда есть ADMIN
- Flutter **18** тестов зелёные (15 виджетов + 3 кейса `BAD_AUTH`, без нового экрана)
- COM23 в списке портов не было (`COM3`–`COM6`, `COM24`, `COM40`, `COM47`) — живой mux / ребут на железе не гоняли

---

## Реализовано раньше (каскад 01–20, кратко)

Access OLED + офлайн ACL, RS-485 UART1 (39/38/26), MESH 485 JSON mux, мост `READ_ONLY`, адаптеры пяти приборов, EventStore + dual ACK, Flutter-консоль без чата, `COMMAND_EXECUTE` только `CONTROL_RELAY` на пустом allow-list PP-001, Сигнал-10/20 MONITOR, фото только с диска телефона/ПК, RAM AuthZ + `BAD_AUTH` + `AUTH_SET`.

Зафиксировано: GPIO **19/20 = USB**; пустой ACL = DENY; OPEN ≠ `COMMAND_EXECUTE`; присутствие в меше **не** авторизация; пожарка жива, если меш умер.

---

## Не реализовано

### Можно брать следующим прогоном

| # | Задача | Почему не сделано | Риск |
|---|---|---|---|
| 1 | **MeshCore pubkey → роль** | **Закрыто в фазе 22** (optional bind). | USB JSON-pubkey не подпись MeshCore. |
| 2 | Живой mux ACL на COM23 | Порта не было в 18–21. | Не доказано: `ACCESS_GET_USERS` после `ACCESS_ADD_CARD` на железе; persist `AUTH_SET` после ребута узла. |
| 3 | Физический свайп Wiegand | Стенд не собирали. | GRANT/DENY на реальном считывателе не доказан (DENY закрыт пустым ACL + native). |

### Не стартовать без отдельного решения

| Задача | Статус |
|---|---|
| AUTO discovery | `UNKNOWN_CMD`. Протокол шины только вручную. |
| Второй RS-485 / dual UART | Нет. Нужен расширитель. **Не** GPIO 19/20. |
| Postgres + FastAPI CRUD / `PHOTO_DIR` | `/employees` = `{items:[]}`. Фото не в LoRa. |
| OTA / `ACCESS_OTA` | `UNKNOWN_CMD`. Нет подписи прошивки. |
| Native Orion CONTROL (реле Сигнал-10/20) | Нет. CRC ≠ исполнение. |
| Persist `CONFIG_SET` (baud / protocol) | Только RAM. Ребут → `RAW`. |
| `BRIDGE_OPEN` кнопкой оператора во Flutter | Placeholder READ_ONLY. |
| Excel / отчёты | На сервере, не на ESP32. |
| MeshCore чат / контакты / QR | Запрещено. |
| Расписания / группы / праздники | ACL — плоский список UID, cap 64. |
| Крипто EM-Marine / anti-clone | UID клонируемый. |
| Смена BLE PIN `123456` | Lab default. |
| Шифрование `/m485_acl.bin` / `/m485_auth.bin` | SPIFFS без crypto. |
| GRANT-all | Намеренно нет. |
| Сертификация пожарки | Не претендуем. |
| PiDoors PHP / MariaDB | Не брать. |

---

## Следующий прогон

Pubkey bind закрыт в фазе 22: [`report-22-ru.md`](report-22-ru.md). [`22-auth-pubkey.md`](../agent/prompts/22-auth-pubkey.md) — **Done**.
