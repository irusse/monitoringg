PROMPT 24 — С2000-М MONITOR listen-only → MESH 485 events (не чат)

Ты продолжаешь **MESH 485 Extender**. Каскад 01–19 **закрыт**. Фазы **20–23** и **25–28** сделаны. Прогон **28** закрыт фикстурой (`docs/report-28-ru.md`); живые платы не гонялись. Делай **только** этот прогон. Не AUTO, не второй UART, не RS-232 на GPIO, не Postgres/фото, не OTA, не MeshCore chat/contacts/QR/DM, не native Orion CONTROL, не GRANT-all, не fire-disable. Стенд ПЦН+СКУД заново не открывать.

Цель: С2000-М остаётся **мастером** шины «Орион». Heltec **слушает** изолированный RS-485 (DE = RX) и фиксирует **CRC-валидные** кадры как события MESH 485, чтобы оператор и **другой узел-шлюз** видели их в EventStore / `EVENT_LIST` / `m485.event`. **Не** личное сообщение MeshCore, не Companion chat, не «байт UART → пакет чата».

Инструкция по проводам (уже в репо): `docs/c2000m.md`. Прочитать **целиком**.

## Уже есть, не ломать

- GPIO 47 → одно `PCN` / `ALARM` или `NORMAL`. Хоп 59 байт на `EXT-002`, тот же uuid, `source` `LORA`, ack ставит `mesh_ok`. Кадр не трогает GPIO 40.
- Образ монитора: `-D MESH485_NODE_ID=\"EXT-002\" -D MESH485_RDM6300=1`. UART1 RX = GPIO **38** — это считыватель, не шина. Listen-only собирать **без** `MESH485_RDM6300` (нода у панели, UART1 = RS-485, DE в RX). Оба режима на одном UART1 не совмещать.
- DISCOVER уже шесть id: `RAW-001`, `TS-001`, `PP-001`, `S10-001`, `S20-001`, `PCN-001`. Карта остаётся `TS-001`.
- Пустой ACL = DENY. AuthZ 20–22 и persist protocol 23 без поломки. `CONFIG_SET AUTO` отказ.
- GPIO **19/20 = USB**. Фото не в JSON. `vendor/MeshCore` не форкать.

## Продукт (не расширять)

Heltec V4 = MeshCore **транспорт** + MESH 485 **приложение** + RS-485 HAL + PROFILE_ACCESS.

```
С2000-М (мастер Орион) ── A/B ── приборы
                         └── isolator, Heltec DE=RX
                                      │
                              EventStore / EVENT_LIST
                                      │
                         MESH 485 на USB/BLE/LoRa (не DM чата)
                                      │
                         Flutter События  |  второй EXT-узел
```

Пожарка жива, если меш умер.

## Заморожено

- GPIO **19/20 = USB**. RS-485 **39 / 38 / 26**. Не MAX3232 на эти пины. Не RS-232 ±12 В на ESP32.
- Фото никогда на узел / OLED / LoRa / JSON.
- OPEN = `ACCESS_OPEN`. Не `COMMAND_EXECUTE`.
- Пустой ACL = DENY. Default `BRIDGE_OPEN` = `READ_ONLY`.
- `COMMAND_EXECUTE` только `CONTROL_RELAY` PP-001 `R1`…, allow-list по умолчанию пустой.
- AuthZ 20–22 без поломки. `CONFIG_SET AUTO` отказ.
- Не TX Bolid «для красоты». Не второй мастер на шине С2000-М.
- Не форкать `vendor/MeshCore`. Не `ui-new` / `DISPLAY_CLASS`. Не тащить chat из `meshcore-open`.

## Прочитать сначала

- `docs/c2000m.md` (RS-485 vs RS-232, Path A vs listen-only, запрет чата)
- `docs/bolid.md`, `docs/reference/orion-frames.md`, `docs/reference/orion-timings.md`, `docs/reference/orion-path-decision.md`
- `docs/radio-admin.md` (чат ≠ консоль)
- EventStore + `EVENT_LIST` / `EVENT_SUBSCRIBE` (фаза 11)
- `firmware/heltec_v4` HAL RS-485 (DE fail-safe RX)
- `SECURITY.md` §7 независимость панели

## Что сделать

Минимальный **listen-only MONITOR**. Не расшифровка всех функций С2000-М, не CONTROL, не чат.

### Семантика

1. Heltec на линии С2000-М: **только RX**. GPIO 26 не поднимать для опроса панели. Не слать Orion-кадры, чтобы «наполнить» события.
2. Кадр с UART1: если длина/CRC-8/MAXIM как `orion-frames.md` — положить в EventStore. Поля честные: uuid, time, source (RS485), type вроде `ORION_RX` / `BUS`, device_id (см. ниже), hex или короткие addr/cmd **без** выдуманного `FIRE` / `ALARM`, если дамп этого не даёт.
3. Битый CRC / обрывок → не событие пожара, можно счётчик UART error в DIAGNOSTICS (если уже есть).
4. Пустая шина → нет фейковых событий (как сейчас `EVENT_LIST` `[]`).
5. Оператор видит это в Flutter **События** (существующий экран). Между узлами — уже существующая публикация mesh events фазы 11, если она есть; **не** вызывать MeshCore send-private-message / contacts.
6. Path A (`PP-001` / `BOLID_C2000PP`) **не ломать**. Предпочтительная схема объекта по-прежнему С2000-ПП; этот прогон — когда ПП нет и нужен пассивный тап.
7. Идентификатор в DISCOVER: либо события на существующий `RAW-001` с явным type `ORION_RX` (шесть id остаются шестью), либо новый **`C2M-001`** (`BOLID_C2000M`, capabilities только READ/MONITOR, без CONTROL). Если новый id — это **седьмой**, так и написать в документах и тестах. Не называть его шестым и не прятать за «id по-прежнему пять». Не вешать эти кадры на `PCN-001`: контакт ПЦН и кадр шины — разные вещи, CRC не становится `ALARM`.
8. `CONFIG_SET`: явный protocol (например `BOLID_C2000M`) **или** listen поверх текущего RAW, но тогда не TX RAW transaction на ту же шину, пока listen-only. Не AUTO.
9. Не разбирать CXEM в exploit-рецепты. Дампы = векторы CRC.

Не делать: RS-232 драйвер, dual UART, экран чата, QR, пароль на OLED, fire-disable, GRANT-all, native реле С2000-М.

### Flutter

- Экран События уже есть — не плодить «чат с нодой».
- Не contacts / MeshCore Open.
- Тесты: не раздувать UI; fixture может показать одно ORION_RX без живой панели.

### Документы

- Дополнить `docs/c2000m.md` фактом образа (что именно пишется в EventStore).
- `docs/mesh485-protocol.md`, `docs/bolid.md` adapter order, `docs/implementation.md` Phase 24.
- Коротко progress / cascade-report / `docs/report-24-ru.md`.
- `AGENTS.md` / `CASCADE.md`: 24 Done. Следующий прогон снова не назначать.
- `docs/radio-admin.md`: ещё раз — события пожарки ≠ MeshCore DM.

## Проверки

- `py -3.13 -m platformio run -e native_protocol_tests` → `program.exe` → `ok`. Не два `heltec_v4_access` параллельно.
- Flutter test зелёные.
- Native: кадр-фикстура CRC OK → событие в EVENT_LIST; мусор CRC → нет ALARM; пусто → `[]`; DE/TX не вызываются в listen-режиме (мок порта).
- Опционально на стенде: isolator на A/B С2000-М, DE RX, события в `m485_devices.py events` или Flutter. Нет панели — не блокировать. Образ без `MESH485_RDM6300`.
- Path A PP-001 / S10 / S20 / Access / AuthZ 20–22 / хоп ПЦН / RDM6300 не сломать.
- GPIO 19/20 и фото не в JSON.
- Образ с `-D MESH485_RDM6300=1` этот прогон не перешивать под шину.

## Запрещено

- MeshCore chat / private message / contacts / QR как транспорт журнала С2000-М.
- Форк MeshCore, TX на шину как второй мастер.
- RS-232 на 19/20/38/39.
- AUTO, OTA, dual UART, Excel, PiDoors, native CONTROL, fire-disable.
- Выдавать CRC OK за «зона в пожаре».
- Exploit-рецепты CXEM.

## Готово когда

1. Listen-only: валидный Orion-кадр → EventStore / EVENT_LIST, без фейкового FIRE.
2. Нет TX с Heltec на шину С2000-М в этом режиме.
3. Оператор и второй шлюз получают **MESH 485 event**, не DM чата.
4. Документы честно: RS-232 не на пины; панель независима.
5. Native + Flutter зелёные.

Потом **стоп**. Не чат, не CONTROL С2000-М, не AUTO, не OTA, не второй UART.
