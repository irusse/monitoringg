# Отчёт по каскаду MESH 485 Extender (01–19 + фазы 20–23, 25–26)

Дата: **13 октября 2026**. Каскад **01–19 закрыт**. Фазы **20–23** и **25–28** (контакт ПЦН, хоп, RDM6300 в ACL, совместная фикстура) сделаны. Образ: `heltec_v4_access` на Heltec V4. Транспорт — MeshCore, прикладной слой — MESH 485. Шесть устройств в DISCOVER: `RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`, `PCN-001`.

Этот файл — человекочитаемый статус. Аудит безопасности: [`SECURITY.md`](../SECURITY.md). Радио vs консоль: [`radio-admin.md`](radio-admin.md).

## Что получилось в целом

На одной прошивке есть:

- Access OLED (русский UI, 10 с гашения) + офлайн ACL турникета
- изолированный RS-485 UART1 (TX 39 / RX 38 / DE 26)
- MESH 485 JSON (USB/BLE mux: `{` → dispatcher, иначе Companion)
- мост по умолчанию **READ_ONLY**
- реестр адаптеров + очередь событий + dual ACK
- Flutter-консоль (объекты, приборы, события, Меш-здоровье, Access) **без чата**

Узел — **шлюз интеграции**, не сертифицированный пожарный прибор. Потеря LoRa / приложения не должна останавливать С2000 / Сигнал-10 / Сигнал-20.

---

## Реализовано (по фазам)

| # | Тема | Что есть в образе |
|---|---|---|
| 01 | Пинout | Карта Access заморожена. GPIO **19/20 = USB**. RS-485 не на USB. |
| 02 | Companion | BLE + USB в одном образе. OLED принадлежит Access, не MeshCore UI. |
| 03 | OLED | READY / GRANT / DENY / REMOTE / LOCK / АВАРИЯ / меню / INFO. `M●` / `R`. DISPLAYOFF 10 с. |
| 04 | Wiegand + турникет | D0/D1 15/16, OPEN 40, лампы 41/33, датчики 3/4, E-STOP 6. |
| 05 | RS-485 HAL | UART1 9600 8N1, DE HIGH только на TX. |
| 06 | MESH 485 | JSON envelope, `RS485_TRANSACTION`, USB mux. |
| 07 | Мост | `BRIDGE_*`. Default `READ_ONLY`. `RAW_BRIDGE` только явный. `CONTROL` — compile flag. |
| 08 | Адаптеры | Device / Point / Event. Клиент не видит опкоды Болида. |
| 09 | Orion freeze | Кадр + CRC-8/MAXIM + тайминги. CXEM = векторы, не прошивка. |
| 10 | С2000-ПП | `PP-001` Modbus READ_ONLY probe. Пустая шина → `online: false` / TIMEOUT. |
| 11 | События | EventStore, dual ACK (`mesh_ok` / `device_ok`). |
| 12–13 | Flutter | Одна консоль, RU UI, живой USB/BLE, fixture без порта. |
| 14 | CONTROL | `COMMAND_EXECUTE` только `CONTROL_RELAY` на именованных `R1`…`R8` PP-001. Default allow-list **пустой**. Запрет fire-disable / disarm-all. |
| 15 | Сигнал-10 | `S10-001` MONITOR. Нет CONTROL. `CONFIG_SET protocol` в RAM, без AUTO. |
| 16 | Сигнал-20 | `S20-001` MONITOR. CRC OK ≠ реле. Без AUTO. |
| 17 | ACL + 3MAP | Офлайн `/m485_acl.bin`, cap 64, DENY по умолчанию. `ACCESS_STATUS/OPEN/LOCK/GET_EVENTS/ADD/REMOVE`. OPEN ≠ `COMMAND_EXECUTE`. |
| 18 | Access UI | Сотрудники / карты / фото **с диска** / OPEN+LOCK / события. `ACCESS_GET_USERS` без байтов фото. |
| 19 | Hardening | `SECURITY.md` + `docs/radio-admin.md`. Проверка: фото не в JSON, GPIO 19/20 свободны. Flutter **15** тестов зелёные. |
| 20 | AuthZ | RAM allow-list `{name, role}` cap 8. Factory `viewer`/`operator`/`admin`. `BAD_AUTH` живой. `AUTH_SET` только ADMIN. Replay 30/120 без изменений. |
| 21 | Persist | `/m485_auth.bin` (blob `AUT1`, как ACL). `AUTH_SET` переживает ребут. Битый файл / нет ADMIN → factory. Файл не шифрован. |
| 22 | Pubkey | Опциональный 32-байт pubkey на строке. Bound без совпадения → `BAD_AUTH`. v1 blob грузится unbound. JSON-pubkey ≠ подпись MeshCore. |
| 23 | Persist protocol | `/m485_cfg.bin` (blob `CFG1`). `CONFIG_SET protocol` переживает ребут. Битый / AUTO-в-файле → `RAW`. Baud по-прежнему отказ. Файл не секрет. |
| 25 | Контакт ПЦН | GPIO 47, дребезг 50 мс. Фронт → одно `PCN`/`ALARM` или `NORMAL` в EventStore. `READ_STATUS` only. |
| 26 | Хоп ПЦН | 59-байтовый кадр на `EXT-002`, `source` `LORA`, тот же uuid. Ack ставит `mesh_ok`. Не чат. GPIO 40 молчит. |
| 27 | RDM6300 | Кадр на GPIO 38 → ACL `TS-001`. Пустой список — `LOCKED`. Седьмого id нет. |
| 28 | Стенд ПЦН + СКУД | Фикстура: тревога не открывает турникет, карта из ACL открывает при висящей тревоге, тишина радио не двоит uuid. Платы не гонялись. [`report-28-ru.md`](report-28-ru.md). |

### Зафиксированные факты продукта (не «хотелки»)

- Присутствие в меше **не** авторизация. VIEWER не OPEN/LOCK/CONTROL.
- Replay: повтор `(id, operator)` 30 с; `ts` вне ±120 с при известном unix time.
- Фото не на LoRa, OLED, SPIFFS, MESH 485 JSON.
- `ACCESS_*` не TX UART1. Адаптеры Болида не становятся OPEN турникета.
- Протокол шины вручную: `RAW` \| `BOLID_C2000PP` \| `BOLID_SIGNAL10` \| `BOLID_SIGNAL20`.

---

## Не реализовано (остатки после каскада)

Разделено: **можно брать следующим прогоном** vs **не начинать без отдельного решения**.

### Следующий осмысленный шаг (рекомендуется)

| Задача | Почему не сделано | Риск, если оставить |
|---|---|---|
| С2000-М listen-only → EventStore | Отдельный прогон: [`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md). Проводка шины: [`c2000m.md`](c2000m.md). | Не чат MeshCore; не второй мастер на шине. |
| Стенд двух плат | Фаза 28 закрыта фикстурой ([`report-28-ru.md`](report-28-ru.md)). Живые платы, эфир и модуль не подключались. | Пока нет стенда, «После 28» в [`test-pcn-acs.md`](test-pcn-acs.md) на железе не доказан. |
| Подпись MESH 485 пакетов MeshCore | Не этот прогон. Equality `payload.pubkey` уже есть. | JSON-ключ на USB по-прежнему подделывается. Factory unbound публичны. |
| Живой mux ACL на COM23 | Порта не было в прогонах 18–20. | Не проверено: `ACCESS_GET_USERS` после `ACCESS_ADD_CARD` на железе. |
| Физический свайп Wiegand | Стенд не делали. DENY закрыт пустым ACL + native. | GRANT/DENY на реальном считывателе не доказан. |

### Отложено сознательно (не стартовать из «первого» промпта)

| Задача | Статус | Комментарий |
|---|---|---|
| AUTO discovery | Отказ `UNKNOWN_CMD` | Протокол шины только вручную. |
| Второй RS-485 / dual UART | Нет | Нужен расширитель. **Не** GPIO 19/20. |
| Postgres + FastAPI CRUD сотрудников / `PHOTO_DIR` | `/employees` = `{items:[]}` | Фото остаются на диске телефона/ПК. Не тащить jpeg в LoRa. |
| OTA / `ACCESS_OTA` | `UNKNOWN_CMD` | Нет подписи прошивки в образе. |
| Native Orion CONTROL (Сигнал-10/20 реле) | Нет | Дампы не дают opcode. CRC ≠ исполнение. |
| Persist `CONFIG_SET` baud в NVS | Только отказ | Baud не в `/m485_cfg.bin`. Protocol persist — фаза 23 (SPIFFS, не NVS). |
| `BRIDGE_OPEN` как действие оператора во Flutter | Placeholder READ_ONLY | Мост не кнопка оператора. |
| Excel / отчёты | Нет | Живут на сервере, не на ESP32. |
| MeshCore чат / контакты / QR | Запрещено | Консоль — не мессенджер. |
| Расписания / группы доступа / праздники | Нет | ACL — плоский список UID, cap 64. |
| Крипто EM-Marine / anti-clone | Нет | UID клонируемый; так и записано в аудит. |
| Смена BLE PIN `123456` | Lab default | Только системный диалог Android. |
| Шифрование `/m485_acl.bin` / `/m485_auth.bin` / `/m485_cfg.bin` | Нет | SPIFFS без crypto. `/m485_cfg.bin` не секрет (выбранный protocol шины). |
| Сертификация пожарки | Не претендуем | Панель независима. |
| GRANT-all | Намеренно нет | Пустой ACL = DENY. |
| PiDoors PHP / MariaDB | Не брать | Только идеи экранов/полей. |

---

## Проверки на момент фазы 23

- Flutter: виджеты 15 + кейсы `BAD_AUTH`, `C:\flutter\bin\flutter.bat test` из `app/mesh485` (без нового экрана).
- Native: persist protocol fake-fs reboot (SIGNAL10 / C2000PP / SIGNAL20); `AUTO` не создаёт и не портит файл; битый / AUTO-в-блобе → `RAW`; VIEWER `NO_PERMISSION`; factory admin OPEN / `eve` `BAD_AUTH`.
- Фото: в `protocol/mesh485/examples/`, EventStore, `ACCESS_GET_USERS` нет `photo` / jpeg.
- `pins.h`: USB 19/20 не используются Access/RS-485.
- Пять DISCOVER id без изменений.

---

Полный отчёт фазы 20: [`report-20-ru.md`](report-20-ru.md). Фаза 21: [`report-21-ru.md`](report-21-ru.md). Фаза 22: [`report-22-ru.md`](report-22-ru.md). Фаза 23: [`report-23-ru.md`](report-23-ru.md).

## Промпт следующего прогона

[`28-pcn-acs-bench.md`](../agent/prompts/28-pcn-acs-bench.md) — **Done** (фикстура). Следующий прогон **не назначен**. Открытым остаётся [`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md) — listen-only шины, если оператор его выберет.

Не AUTO, не chat/QR, не второй UART, не OTA, не фото в радио, не native Orion CONTROL. Не слать журнал С2000-М личным сообщением MeshCore.
