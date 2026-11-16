# Отчёт о прогрессе — MESH 485 Extender

Дата: **13 октября 2026**. Каскад **01–19** закрыт. Фазы **20–23** и **25–28** сделаны. Следующий прогон **не назначен**: оператор выбирает listen-only [`agent/prompts/24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md) или остановку. Стенд 28 — фикстура, платы не гонялись: [`report-28-ru.md`](report-28-ru.md), [`test-pcn-acs.md`](test-pcn-acs.md). С2000-М: [`c2000m.md`](c2000m.md).

## Короткий вывод

На Heltec V4 уже есть **транспорт + HAL + протокол + два stub-адаптера + заморозка Orion**. Клиент видит Device / Point / Event (`RAW-001`, `TS-001`), а не опкоды Болида. Живого С2000-ПП и панели на стенде **нет** — пустой A/B по-прежнему не должен порождать ложные события. Фаза 10 — первый **read-only** адаптер Path A (Modbus RTU к С2000-ПП) со stub, который на пустой шине честно уходит в `TIMEOUT` / `online: false`.

Критической ошибки архитектуры нет. CRC-8 Ориона проверен тестами и **не** подключён к UART. Управление пожаркой / `COMMAND_EXECUTE` по-прежнему запрещены.

## Стек сейчас

```
MeshCore (транспорт, BLE+USB Companion)
  → MESH 485 JSON (USB mux: «{» → dispatcher)
      → Bridge READ_ONLY | RAW_BRIDGE
      → DeviceRegistry
           RAW-001  RawRs485Adapter     (не TX)
           TS-001   AccessTurnstileAdapter
           (фаза 10)  С2000-ПП Modbus READ_ONLY stub
      → Rs485Hal UART1 TX39 / RX38 / DE26
  → PROFILE_ACCESS (Wiegand / турникет)
  → crc8_orion  — только native-тесты, не mux
```

## Что закрыто (01–09)

| Фаза | Результат | Доказательство |
|---|---|---|
| 01 Пинout | GPIO заморожены; USB **19/20** не трогать | `docs/pinout.md`, `include/access/pins.h` |
| 02 Companion | BLE+USB MeshCore, OLED свой | `heltec_v4_access`, без `DISPLAY_CLASS` |
| 03 OLED | READY / меню / 10 с DISPLAYOFF | VEXT GPIO 36 **LOW** = питание |
| 04 Access | Wiegand 15/16, OPEN 40, лампы, E-STOP | ACL = stub (любой UID) |
| 05 RS-485 | UART1 9600 8N1, DE HIGH только на TX | `hardware/rs485-adapter.md` |
| 06 Протокол | JSON + `RS485_TRANSACTION` + USB mux | native + образ; COM23 mux проверен в 08 |
| 07 Мост | `BRIDGE_*`, default **READ_ONLY** | TX во время RO запрещён |
| 08 Адаптеры | `DEVICE_*` / `POINT_*` / `EVENT_LIST` | DISCOVER: `RAW-001` + `TS-001` |
| 09 Orion freeze | кадр, CRC-8, тайминги, Path A/B, сущности HA | см. ниже |

Образ `heltec_v4_access` после фазы 09: RAM **8.3%**, flash **18.7%**. Запас большой. Прошивать после 09 **не требовалось** (только CRC + docs).

## Фаза 09 — заморозка Orion (этот прогон)

Цель: чтобы фаза 10 не спорила про CRC и карту кадра. **Не** адаптер, **не** опрос Modbus, **не** CONTROL.

### Кадр Path B (документы, не TX)

| Поле | Где | Смысл |
|---|---|---|
| Адрес | байт 0 | 1…127; в шифрованных дампах `addr \| 0x80` |
| Длина | байт 1 | `длина_кадра − 1` (всё после адреса, включая длину и CRC) |
| Команда / данные | со 2-го байта | коды из дампов **только документированы**, таблица исполнения запрещена |
| CRC | **последний байт** | CRC-8 по всем байтам кроме него |

Источник расклада: опубликованные дампы [Habr 563584](https://habr.com/ru/articles/563584/) со ссылкой на тему CXEM (21.03.2012). Посты CXEM через JS не выгрузились — в репозиторий легли **опубликованные hex**, не расшифрованная прошивка.

### CRC — что совпало

`crc8_orion` = **CRC-8/MAXIM-DOW** (Dallas/Maxim): полином `0x31`, reflected `0x8C`, init 0, xor-out 0. Check `"123456789"` → `0xA1`. Residue по полному кадру → `0x00`.

| Алгоритм | Фикстуры |
|---|---|
| `Crc8Algo::Orion` / `crc8_orion` | **совпал** |
| `Crc8Algo::DallasMaxim` | **совпал** (тот же алгоритм) |
| `Crc8Algo::Generic` (poly `0x07`) | **не совпал** |

Примеры: `03 06 00 11 BA BA 8D`, `83 08 00 ED B8 BA BA BA 62`, `83 0A … D7`. Код: `firmware/heltec_v4/include/protocols/bolid/crc8.h`. В `RS485_TRANSACTION` **не** вшит.

### Тайминги (границы проектирования, не догма)

| Событие | Граница | Зачем узлу |
|---|---|---|
| Смена направления / DE | ~50 мс | HAL уже держит DE после TX; **не** ждать 2 с |
| Ответ на событие | ~800 мс | default транзакции **900** мс вписывается |
| Команда | ~900 мс | потолок **2000** |
| Поиск | ~600 мс | AUTO не в этом прогоне |

Формулы С2000-РПИ при R=0 короче (30 / 360 / 660 / 90 мс). **`RAW_BRIDGE` — не путь Ориона**: RTT меша ≠ 50 мс UART; байтовый стрим по LoRa/Companion разъезжается с панелью. Постоянный путь — кадровая транзакция. `READ_ONLY` мост только слушает.

### Path A vs Path B для фазы 10

Стенд: Heltec **COM23**, изолированный UART1, USB-RS485 для петли. **Нет С2000-ПП, нет C2000 / Signal-20.**

**Решение:** Path A как продуктовый default + **Modbus RTU read stub**, который на пустой шине даёт `device_ok: false` / `TIMEOUT`. Не Path B MONITOR «который никогда не TX» (это был бы фейковый `BOLID_*` без событий). Файл: `docs/reference/orion-path-decision.md`. Карта чтения: `docs/reference/c2000pp-modbus.md` (РЭп АЦДР.426469.020, Изм.17).

### hass-hubc2000pp

Клон: `vendor/hass-hubc2000pp` (Apache-2.0). Имена зон / реле / разделов и PING:22000 / UDP push :+1 — в `docs/reference/hubc2000pp-entities.md`. **Не** копировать HA-компонент и TCP-стек в ESP32.

### Проверки 09

- `py -3.13 -m platformio run -e native_protocol_tests` → `native_protocol_tests: ok` (кейсы 06–08 + CRC).
- Образ `heltec_v4_access` собран. Первая параллельная сборка упала на гонке `.pio` (два `pio run` сразу); повтор — **SUCCESS**.
- Mux / `DEVICE_DISCOVER` не менялись: по-прежнему два stub. `COMMAND_EXECUTE` = `NO_PERMISSION`.

## Проблемы (не стопперы кода)

1. **`pio` нет в PATH этой оболочки** — вызывать `py -3.13 -m platformio`. Не собирать `heltec_v4_access` дважды параллельно.
2. **Пустая шина A/B** — `TIMEOUT` и пустой `EVENT_LIST` правильны. Живой ACK не выдумывать. Изолятор обязателен до живой панели.
3. **Мост READ_ONLY** и адаптер, который сам ходит в UART, не должны TX одновременно — это правило фазы 10.

### Заглушки по каскаду (не баги)

- ACL: офлайн DENY-by-default (фаза 17). Lab UID `04A79122` через `allowUid` / `MESH485_ALLOW_TEST_UID`.
- Authz: ADMIN stub до 14/19.
- `COMMAND_EXECUTE` (кроме named CONTROL_RELAY на PP-001), persist `CONFIG_SET` (кроме RAM `protocol`), `ACCESS_*` — отказ. `AUTO` discovery — отказ.
- BLE NUS не mux’ит JSON.
- Протокол в `CONFIG_GET` = `RAW` (нет NVS).
- GPS снят (`ENV_INCLUDE_GPS=0`): 38/39 = RS-485.

## Готовность к фазе 10

Есть реестр, HAL, карта регистров С2000-ПП и решение Path A. Нет преобразователя на столе — адаптер должен **зарегистрироваться**, сделать документированный **read** (probe FC03 @ 46152) и при тишине шины остаться `online: false` без фейковых зон «Пожар». Клиенты по-прежнему видят Point (`zone` / `relay` / `section`), не «команда 0x17».

## Фаза 15 — Signal-10 MONITOR (2026-08-29)

Четвёртый адаптер `S10-001` / `BOLID_SIGNAL10`. Path B: запрос состояния Орион (Habr `0x57`/`0x02`, CRC-8/MAXIM). Пустая шина → `online: false`, точка `S10` `FAULT`, без «Пожар». CONTROL нет (дампы не дают реле). `CONFIG_SET protocol` в RAM: `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10`, **без AUTO**. На UART TX только выбранный адаптер.

Проверки: `native_protocol_tests: ok`; образ `heltec_v4_access` (RAM 9.2% / flash 19.0%) прошит на **COM23**. Mux: `devices:4`, DISCOVER четыре id, `CONFIG_SET AUTO` → `no AUTO`. Flutter: 10 тестов.

## Фаза 16 — Signal-20 MONITOR (2026-08-29)

Пятый адаптер `S20-001` / `BOLID_SIGNAL20`. Тот же Orion status-read, что у Signal-10 (Habr `0x57`/`0x02`, CRC-8/MAXIM). Пустая шина → `online: false`, точка `S20` `FAULT`, без «Пожар». CONTROL нет (дампы не дают opcode реле). CRC OK ≠ реле щёлкнуло (программа реле в приборе). `CONFIG_SET protocol` в RAM: `RAW` | `BOLID_C2000PP` | `BOLID_SIGNAL10` | `BOLID_SIGNAL20`, **без AUTO**.

Проверки: `native_protocol_tests: ok`; образ `heltec_v4_access` (RAM 9.2% / flash 19.0%) прошит на **COM23**. Mux: `devices:5`, DISCOVER пять id, `CONFIG_SET AUTO` → `no AUTO`. Flutter: 11 тестов.

## Фаза 17 — ACL + 3MAP (2026-08-29)

Офлайн ACL вместо GRANT-all фазы 04. Пустой список / неизвестный / disabled UID = **DENY** (красная лампа, `showDenied`, без импульса GPIO 40). Попадание в ACL = ALLOW (зелёная + OPEN + `showGranted`). UID — канонический uppercase hex без пробелов. Persist SPIFFS `/m485_acl.bin`, cap 64, overflow `BUSY` (без silent wrap). Записи ACL на **куче** (не BSS): иначе первая прошивка паниковала в C2000-PP `poll` → `Serial1.write`. Lab: `allowUid` / `-D MESH485_ALLOW_TEST_UID=1` (`04A79122`). UID не крипто.

MESH 485: `ACCESS_STATUS` (VIEWER), `ACCESS_OPEN` / `ACCESS_TURNSTILE_OPEN` (OPERATOR+, dual ACK, OLED `showRemote`, событие `REMOTE_OPEN`), `ACCESS_LOCK`, `ACCESS_GET_EVENTS` (тот же `EventStore`, фильтр TS-001), ADMIN `ACCESS_ADD_CARD` / `ACCESS_REMOVE_CARD`. VIEWER mutate/OPEN → `NO_PERMISSION` (`mesh_ok: true`, `device_ok: false`). `ACCESS_OTA` → `UNKNOWN_CMD`. `COMMAND_EXECUTE` не OPEN. `ACCESS_*` не TX UART1. TS-001: `READ_STATUS` + `ACCESS_OPEN`. INFO users = число enabled записей. Пять DISCOVER id без изменений.

Flutter: OPERATOR+ OPEN с confirm → live `ACCESS_OPEN`. VIEWER кнопка disabled. Fixture ACK честный TIMEOUT. Сотрудники/фото — фаза 18.

Проверки: `native_protocol_tests: ok`; Flutter 12 тестов. Образ `heltec_v4_access` (RAM **9.2%** / flash **19.1%**) прошит на **COM23**, boot стабилен (>2 мин). Mux: `devices:5`, DISCOVER пять id, Bolid `online: false` на пустой шине. Пустой ACL `users:0`. Admin OPEN dual ACK `state: OPEN`. VIEWER OPEN/LOCK/ADD → `NO_PERMISSION`. ADMIN add/remove card меняет `users`. `ACCESS_GET_EVENTS` видит UUID `REMOTE_OPEN` `source=USB`. `COMMAND_EXECUTE ACCESS_OPEN` → `NO_PERMISSION`. Физический свайп Wiegand на стенде не делали — DENY-default закрыт пустым ACL + native.

## Фаза 18 — сотрудники в том же Flutter (2026-08-29)

Экран Доступ: сотрудники/карты против ACL узла, фото **только с диска**, OPEN + LOCK, события TS-001. `ACCESS_GET_USERS` (ADMIN dump без байтов фото). VIEWER не OPEN/LOCK и не меняет карты. Fixture OPEN/LOCK = честный TIMEOUT; ADMIN add-card в RAM fixture. FastAPI `/employees` по-прежнему пустой stub.

Проверки: `native_protocol_tests: ok`; Flutter **15** тестов. Образ `heltec_v4_access` (RAM **9.3%** / flash **19.1%**). COM23 в этом прогоне не было в списке портов — mux `acl` не гоняли. Пять DISCOVER id без изменений.

## Фаза 19 — hardening (2026-08-29)

Документы и проверка, без новых фич. `SECURITY.md` — аудит этого образа (не заглушка): UID EM-Marine не крипто, BLE PIN `123456` только в системном диалоге, роли + replay 30/120 с, GPIO **19/20** = USB, фото не в LoRa/OLED/SPIFFS/JSON, OLED без PIN, пожарка жива без узла, dual ACK, `READ_ONLY` / `RAW_BRIDGE` только lab, CXEM = векторы. `docs/security.md` ссылается на корневой файл. `docs/radio-admin.md`: инструменты MeshCore vs экран **Меш**.

Проверки: в goldens / EventStore / `ACCESS_GET_USERS` нет байтов фото; `pins.h` USB 19/20 не Access/RS-485. Flutter тесты без новых экранов.

## Фаза 20 — локальный AuthZ (2026-08-29)

RAM allow-list операторов (cap 8), factory `viewer` / `operator` / `admin`. Роль больше не берётся только из строки клиента. Неизвестный `operator` или заявленная роль выше записи → envelope `BAD_AUTH`. `AUTH_SET` только после ADMIN. Replay 30/120 не тронут. VIEWER OPEN по-прежнему `NO_PERMISSION`. Присутствие на LoRa/BLE/USB не авторизация.

Проверки: `native_protocol_tests: ok`; Flutter тесты (15 виджетов + кейсы BAD_AUTH). Пять DISCOVER id без изменений.

Отчёт: [`report-20-ru.md`](report-20-ru.md). Persist списка — фаза 21.

## Фаза 21 — persist allow-list (2026-08-30)

`AUTH_SET` переживает ребут. SPIFFS `/m485_auth.bin` (magic `AUT1`, как ACL-блоб, не NVS, не шифрован) + native fake-fs. Нет файла / битый magic / overflow / нет ADMIN → factory `viewer` / `operator` / `admin` (лог, не молчаливый мусор). Запись только после успешного `AUTH_SET`. `BAD_AUTH` / replay / VIEWER OPEN как в 20. Factory имена по-прежнему публичны.

Проверки: `native_protocol_tests: ok` (lab OPEN после `begin()`, spoof `admin` после remove+persist → `BAD_AUTH`, битый файл → factory); Flutter тесты без нового экрана. Пять DISCOVER id без изменений.

Отчёт: [`report-21-ru.md`](report-21-ru.md). Pubkey bind — фаза 22.

## Фаза 22 — optional pubkey bind (2026-08-30)

Строка `{name, role, pubkey?}`. Unbound factory как в 21. Bound: `payload.pubkey` (64 hex) или `Mesh485Env.sender_pub`. Нет/чужой → `BAD_AUTH`. `/m485_auth.bin` v2; v1 грузится unbound. JSON-pubkey на USB **не** подпись MeshCore.

Проверки: `native_protocol_tests: ok`; Flutter 18 без нового экрана. Пять DISCOVER id без изменений.

Отчёт: [`report-22-ru.md`](report-22-ru.md). Persist `CONFIG_SET protocol` — фаза 23.

## Фаза 23 — persist CONFIG protocol (2026-09-11)

`CONFIG_SET protocol` переживает ребут. SPIFFS `/m485_cfg.bin` (magic `CFG1`, одно поле, как ACL/auth, не NVS, не шифрован) + native fake-fs. Нет файла / битый magic / неизвестный protocol / `AUTO` в файле → `RAW` (лог, не молчаливый мусор). Запись только после успешного `CONFIG_SET` с `protocol`. `AUTO` и baud persist по-прежнему отказ. AuthZ 20–22 без изменений.

Проверки: `native_protocol_tests: ok` (SIGNAL10/PP/S20 после `begin()`, AUTO не портит файл, битый блоб → RAW); Flutter тесты без нового экрана. Пять DISCOVER id без изменений.

Отчёт: [`report-23-ru.md`](report-23-ru.md). Следующий прогон — [`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md). Проводка С2000-М: [`c2000m.md`](c2000m.md) (не чат MeshCore).

## После каскада (не начинать вместо 24)

AUTO discovery, второй UART, Postgres/фото CRUD, OTA, native Orion CONTROL, MeshCore chat/QR. С2000-М listen-only — прогон 24, не вместо 23.
