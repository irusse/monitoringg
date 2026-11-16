# Отчёт — фаза 20 AuthZ (30 августа 2026)

Продукт: **MESH 485 Extender**. Образ `heltec_v4_access`. Каскад **01–19 закрыт**. Фаза **20** — первый пост-каскадный прогон, **сделана**.

Пять DISCOVER без изменений: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

---

## Реализовано в фазе 20

| Что | Как в образе |
|---|---|
| Роль не из клиента как истина | Envelope `operator` — **имя**. Роль берётся из RAM allow-list `{name, role}` (cap 8). |
| Factory seed | `viewer` / `operator` / `admin` — те же строки, что шлёт Flutter login chip. |
| `BAD_AUTH` живой | Неизвестный `operator` **или** заявленная роль выше записи → envelope `BAD_AUTH`. |
| `AUTH_SET` | Только если запрос уже прошёл как ADMIN. `{name, role}` upsert; `{name, enabled:false}` удаляет (не последнего ADMIN). |
| Replay 30/120 | Не сломан. Проверяется **до** AuthZ. Повтор `(id, operator)` → `REPLAY`. |
| VIEWER | По-прежнему не OPEN / LOCK / ADD_CARD → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`). |
| Factory admin / operator | OPEN / LOCK / ADD как в фазах 17–18. Native OPEN: dual ACK, fake HAL `device_ok: true`. |
| Flutter | Login chip шлёт factory-имя. Fixture: неизвестный operator на mutate/OPEN → `BAD_AUTH`. Без новых экранов. |
| Документы | `SECURITY.md` §3 больше не stub. Протокол, implementation Phase 20, cascade-report, AGENTS / CASCADE. |

### Проверки фазы 20

- `native_protocol_tests: ok`
- Flutter **18** тестов (15 виджетов + 3 кейса `BAD_AUTH`)
- COM23 в списке портов не было — живой mux ACL не гоняли

---

## Реализовано раньше (каскад 01–19, кратко)

Access OLED + офлайн ACL, RS-485 UART1 (39/38/26), MESH 485 JSON mux, мост `READ_ONLY`, адаптеры пяти приборов, EventStore + dual ACK, Flutter-консоль без чата, `COMMAND_EXECUTE` только `CONTROL_RELAY` на пустом allow-list PP-001, Сигнал-10/20 MONITOR, фото только с диска телефона/ПК.

Зафиксировано: GPIO **19/20 = USB**; пустой ACL = DENY; OPEN ≠ `COMMAND_EXECUTE`; присутствие в меше **не** авторизация; пожарка жива, если меш умер.

---

## Не реализовано

### Можно брать следующим прогоном

| # | Задача | Почему не сделано | Риск |
|---|---|---|---|
| 1 | **Persist allow-list** (SPIFFS `/m485_auth.bin`) | **Закрыто в фазе 21.** | — |
| 2 | Живой mux ACL на COM23 | Порта не было в 18–20. | Не доказано: `ACCESS_GET_USERS` после `ACCESS_ADD_CARD` на железе. |
| 3 | Физический свайп Wiegand | Стенд не собирали. | GRANT/DENY на реальном считывателе не доказан (DENY закрыт пустым ACL + native). |
| 4 | MeshCore pubkey → роль | **Закрыто в фазе 22** (optional bind + equality; не подпись пакета). | USB JSON-pubkey всё ещё не подпись. |

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
| Шифрование `/m485_acl.bin` | SPIFFS без crypto. |
| GRANT-all | Намеренно нет. |
| Сертификация пожарки | Не претендуем. |
| PiDoors PHP / MariaDB | Не брать. |

---

## Следующий прогон

Persist закрыт в фазе 21: [`report-21-ru.md`](report-21-ru.md). [`21-auth-persist.md`](../agent/prompts/21-auth-persist.md) — **Done**.
