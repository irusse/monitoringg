# Отчёт — фаза 22 optional pubkey bind (30 августа 2026)

Продукт: **MESH 485 Extender**. Образ `heltec_v4_access`. Каскад **01–19 закрыт**. Фазы **20–21** сделаны. Фаза **22** — третий пост-каскадный прогон, опциональная привязка pubkey, **сделана**.

Пять DISCOVER без изменений: `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001`.

---

## Реализовано в фазе 22

| Что | Как в образе |
|---|---|
| Строка | `{name, role, pubkey?}` cap 8. 32 байта, нули = unbound (factory без ключа). |
| Предъявление | Одно место в JSON: **`payload.pubkey`** (64 hex). Иначе `Mesh485Env.sender_pub`, если транспорт заполнил. |
| Resolve | Unbound — только имя. Bound — нет/чужой ключ → `BAD_AUTH`. Совпадение → роль из таблицы. Claimed-up / неизвестный — как в 20. |
| `AUTH_SET` | `{name, pubkey:"<64 hex>"}` bind; `""` / `null` — unbind, роль не трогать. По-прежнему `{name, role}` и `{name, enabled:false}`. |
| Persist | Write `/m485_auth.bin` version **2**. Файл фазы 21 (version 1) грузится как unbound, не factory-сброс. Без ADMIN не пишем. Не шифрован. |
| `caller_` | После успешного resolve роль берётся из запроса, не повторный `roleOf` без ключа (иначе bound OPEN стал бы VIEWER). |
| Честность | Pubkey в JSON на USB/BLE — **не** подпись MeshCore. Equality check. Factory unbound имена публичны. |
| Flutter | Login chip без изменений. Нет экрана ключей / чата / QR. |
| Документы | `SECURITY.md` §3 + Residual 1. Протокол, implementation Phase 22, cascade-report, AGENTS / CASCADE. |

### Проверки фазы 22

- `native_protocol_tests: ok`
- v1 blob → factory unbound, admin OPEN dual ACK (без сброса в factory-лог)
- bind `lab` → OPEN без ключа `BAD_AUTH`; тот же hex OK; чужой hex `BAD_AUTH`; `env.sender_pub` OK
- persist reboot держит bind; unbind → OPEN без ключа
- VIEWER OPEN / `eve` как в 20–21
- Flutter **18** тестов зелёные (без нового экрана)
- COM23 в этом прогоне не гоняли

---

## Реализовано раньше (каскад 01–21, кратко)

Access OLED + офлайн ACL, RS-485 UART1 (39/38/26), MESH 485 JSON mux, мост `READ_ONLY`, адаптеры пяти приборов, EventStore + dual ACK, Flutter-консоль без чата, `COMMAND_EXECUTE` только `CONTROL_RELAY` на пустом allow-list PP-001, Сигнал-10/20 MONITOR, фото только с диска, RAM AuthZ + `BAD_AUTH` + persist `/m485_auth.bin`.

Зафиксировано: GPIO **19/20 = USB**; пустой ACL = DENY; OPEN ≠ `COMMAND_EXECUTE`; присутствие в меше **не** авторизация; пожарка жива, если меш умер.

---

## Не реализовано

### Можно брать следующим прогоном

| # | Задача | Почему не сделано | Риск |
|---|---|---|---|
| 1 | **С2000-М listen-only → EventStore** | Следующий прогон 24. | Не чат MeshCore; не второй мастер. |
| 2 | Живой mux ACL на COM23 | Порта не было в 18–22. | Не доказано: `ACCESS_GET_USERS` после `ACCESS_ADD_CARD`; persist auth после ребута узла. |
| 3 | Физический свайп Wiegand | Стенд не собирали. | GRANT/DENY на реальном считывателе не доказан. |
| 4 | Подпись MESH 485 пакетов MeshCore | Нет sender в mux без форка `vendor/`. | JSON-`pubkey` на USB по-прежнему подделывается. Factory unbound публичны. |

### Не стартовать без отдельного решения

| Задача | Статус |
|---|---|
| AUTO discovery | `UNKNOWN_CMD`. Протокол шины только вручную. |
| Persist baud | По-прежнему отказ (тест 15). Не в одном прогоне с protocol. |
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
| Шифрование `/m485_acl.bin` / `/m485_auth.bin` | SPIFFS без crypto. |
| GRANT-all | Намеренно нет. |
| Сертификация пожарки | Не претендуем. |
| PiDoors PHP / MariaDB | Не брать. |

---

## Следующий прогон

[`23-config-persist.md`](../agent/prompts/23-config-persist.md) — **Done** ([`report-23-ru.md`](report-23-ru.md)). Следующий: [`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md).
