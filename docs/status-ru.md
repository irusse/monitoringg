# Статус MESH 485 Extender (13 октября 2026)

Продукт: **MESH 485 Extender** на Heltec WiFi LoRa 32 V4. MeshCore — транспорт. Прикладной слой — MESH 485. Турникет и Болид — профили на **одном** образе `heltec_v4_access`, не отдельные прошивки.

Каскад **01–19 закрыт**. Пост-каскад **20–23** и **25–28** сделаны (контакт ПЦН, хоп, RDM6300 в ACL `TS-001`, совместная фикстура). Следующий прогон **не назначен**. Прогон 24 (listen-only шины «Орион») остаётся отдельным и не стартует сам. Отчёт стенда: [`report-28-ru.md`](report-28-ru.md). Проверка: [`test-pcn-acs.md`](test-pcn-acs.md).

Узел — **шлюз интеграции**, не сертифицированный пожарный прибор и не high-security ACS. Потеря LoRa / USB / приложения **не** должна останавливать С2000 / Сигнал-10 / Сигнал-20.

Шесть DISCOVER id: прежние пять `RAW-001` / `TS-001` / `PP-001` / `S10-001` / `S20-001` и `PCN-001` (только чтение). Node id MESH 485: `EXT-001` (OLED может показывать `TS-001`).

Канонические документы: [`architecture.md`](architecture.md), [`pinout.md`](pinout.md), [`mesh485-protocol.md`](mesh485-protocol.md), [`c2000m.md`](c2000m.md) (С2000-М: RS-485, не чат MeshCore), [`SECURITY.md`](../SECURITY.md), [`cascade-report-ru.md`](cascade-report-ru.md).

---

## Что это за система

```
Консоль оператора (Flutter, без чата)     MeshCore tools (ключи / CLI)
        │                                        │
   MESH 485 JSON NDJSON                      Companion binary
        │
Heltec V4  OLED + ACL + Auth + EventStore + UART1 + repeater
        │
   изолированный RS-485              Wiegand / реле / лампы
        │                                    │
   С2000-ПП / Сигнал-10 / Сигнал-20     турникет + считыватель
```

Слои (не пропускать): MeshCore → MESH 485 JSON → Device / Point / Event → адаптер → HAL (UART1 или GPIO).

Mux USB и BLE: строка, которая начинается с `{`, — MESH 485. Всё остальное — Companion. Не оборачивать JSON в кадры `0x3c`/`0x3e`. После открытия CDC плата часто ресетится — подождать ~2.5 с, пропустить boot-строки без `{`.

---

## Реализовано (можно считать «есть в образе»)

### Транспорт и узел

| Возможность | Статус |
|---|---|
| BLE + USB Companion в одном бинарнике | Есть. BLE имя `3MESH-TS-001`, PIN `123456` (системный диалог Android, не OLED). |
| OLED Access UI (русский) | READY / GRANT / DENY / REMOTE / LOCK / АВАРИЯ / меню / INFO. Гашение 10 с. |
| Repeater | По умолчанию вкл., persist SPIFFS, меню USER. Ключи радио — MeshCore tools, не Flutter. |
| GPIO 19/20 | USB PHY. Access и RS-485 их не трогают. |
| RS-485 HAL | UART1 TX **39** / RX **38** / DE **26**, 9600 8N1. DE HIGH только на TX. |

### MESH 485

| Op / механизм | Кто | Поведение |
|---|---|---|
| `NODE_STATUS` / `DIAGNOSTICS` | VIEWER | Здоровье узла, dual ACK last_command, session |
| `DEVICE_DISCOVER` / INFO / STATUS / POINT_* | VIEWER | Пять приборов |
| `EVENT_LIST` / `EVENT_SUBSCRIBE` | VIEWER | Очередь + USB NDJSON `m485.event` |
| `RS485_TRANSACTION` | ADMIN-ish (как CONFIG) | Кадр туда-обратно, не «байт на LoRa» |
| `BRIDGE_OPEN` default `READ_ONLY` | ADMIN stub | Захват RX, **без** TX. `RAW_BRIDGE` только явный. `CONTROL` — compile flag |
| `COMMAND_EXECUTE` | OPERATOR+ | Только `CONTROL_RELAY` на PP-001 `R1`…`R8`. Default allow-list **пустой** (нет TX) |
| `ACCESS_*` | см. роли | Турникет: STATUS/OPEN/LOCK/события/карты. Не UART1 |
| `AUTH_SET` | ADMIN | Allow-list `{name, role, pubkey?}`, persist `/m485_auth.bin` |
| `CONFIG_SET protocol` | ADMIN | Persist `/m485_cfg.bin`: `RAW` \| `BOLID_C2000PP` \| `BOLID_SIGNAL10` \| `BOLID_SIGNAL20`. **`AUTO` отказ**. Baud persist нет. |
| Replay | все | Дубль `(id, operator)` 30 с → `REPLAY`; `ts` ±120 с если unix известен |
| Dual ACK | команды к железу | `mesh_ok` = дошло до узла; `device_ok` = HAL/шина честно ответила; пустая шина = `TIMEOUT` |

Роли: envelope `operator` — **имя**. Истина — таблица на узле (factory `viewer` / `operator` / `admin`). Неизвестный / claimed-up / bound без ключа → `BAD_AUTH`. VIEWER не OPEN/LOCK/ADD. Присутствие на LoRa/BLE/USB **не** авторизация.

### Приборы (адаптеры)

| Id | Протокол | Что умеет сейчас | Чего нет |
|---|---|---|---|
| `RAW-001` | RAW_RS485 | Точка BUS, не TX сам | Не турникет |
| `TS-001` | ACCESS | Wiegand ACL, OPEN GPIO 40, LOCK, события | Фото, расписания, крипто карт |
| `PP-001` | BOLID_C2000PP | Path A: Modbus probe FC 0x03. `online` только после CRC-valid read | Default нет `CONTROL_RELAY`. Нужен `-D MESH485_ALLOW_PP_RELAY=1` или `allowRelay` для R1 |
| `S10-001` | BOLID_SIGNAL10 | MONITOR status-read Orion. TX только если CONFIG = SIGNAL10 | Нет CONTROL реле |
| `S20-001` | BOLID_SIGNAL20 | То же MONITOR | CRC OK ≠ реле сработало |

Пустая шина A/B: Bolid `online: false`, честный `TIMEOUT`, **не** фейковый пожар.

### Flutter (`app/mesh485`)

Одна консоль Android = Windows. Экраны: логин (роль), дашборд, объекты, приборы, события, Control, мост (placeholder READ_ONLY), Меш (RSSI/SNR/uptime, **не чат**), Access (сотрудники/карты/фото с диска/OPEN+LOCK), Settings (COM/BLE).

Без порта — **fixture** JSON (UI тестируется). С портом — живой USB/BLE mux.

18 автотестов зелёные (виджеты + `BAD_AUTH`).

---

## Что уже можно тестировать (без самообмана)

Уровни доказанности:

1. **Native / Flutter CI** — всегда. Не нужен Heltec.
2. **Живой узел USB** — нужен прошитый Heltec V4 (в отчётах стенд: COM23, когда порт есть).
3. **Шина / турникет** — нужен соответствующий hardware. Часть этого **ещё не гоняли** на железе.

### Уже закрыто тестами в репозитории

```
cd firmware/heltec_v4
py -3.13 -m platformio run -e native_protocol_tests
.pio\build\native_protocol_tests\program.exe
# ждать: native_protocol_tests: ok

cd app/mesh485
C:\flutter\bin\flutter.bat test
# 18 passed
```

Native покрывает: envelope, шесть DISCOVER, мост READ_ONLY, Bolid CRC, пустая шина TIMEOUT, ACL persist fake-fs, AuthZ `BAD_AUTH` / VIEWER `NO_PERMISSION`, persist allow-list, pubkey bind, persist CONFIG protocol, replay, фронт GPIO 47 (`PCN` / `ALARM` / `NORMAL`).

Flutter: логин, DISCOVER id, dual ACK в событиях, VIEWER не OPEN, OPERATOR OPEN/LOCK fixture TIMEOUT, ADMIN add card в fixture, `BAD_AUTH`.

### Можно тестировать на одном Heltec (USB, без шины и без считывателя)

Прошить `heltec_v4_access`. Баннер: `3MESH ACCESS`, `BLE name: 3MESH-TS-001`, `USB CDC ready`. OLED: ГОТОВ / ПРИЛОЖИТЕ КАРТУ.

Скрипты (после ресета CDC ~2.5 с):

```
python scripts/m485_devices.py COM23 all
python scripts/m485_node_status.py COM23
python scripts/m485_bridge.py COM23 open   # READ_ONLY
```

Ожидаемо:

- DISCOVER пять id; Bolid `online: false` на пустой A/B
- `ACCESS_STATUS` `users:0` (пустой ACL = DENY)
- ADMIN `ACCESS_OPEN` → dual ACK, OLED REMOTE (GPIO 40 пульс, если реле повешено; без реле всё равно mesh_ok)
- VIEWER OPEN → `NO_PERMISSION`
- неизвестный `operator` → `BAD_AUTH`
- `ACCESS_ADD_CARD` + `ACCESS_GET_USERS` — **долг 18–22**, если COM23 свободен: именно это ещё не доказано на железе в последних прогонах
- `CONFIG_SET AUTO` → отказ
- Flutter с USB: те же ops с логином viewer/operator/admin
- USER: короткое нажатие — демо экранов; длинное — меню / repeater

**Не ожидать:** живой GRANT по карте (нет считывателя); `PP-001 online: true` (нет С2000-ПП); CONTROL реле (allow-list пустой).

### Можно тестировать с изолированным RS-485 + USB-RS485 dongle (петля)

Схема: Heltec 39/38/26 → isolator → A/B → USB-RS485 на ПК, 9600 8N1.

- Idle RX на OLED `RS485 N B`
- `RS485_TRANSACTION` / `RAW_BRIDGE` txn (явный RAW_BRIDGE)
- `BRIDGE_POLL` в READ_ONLY видит кадры с dongle
- Не путать с Orion: RAW кадры ≠ «панель выполнила команду»

### Можно тестировать турникет (когда соберёте стенд)

Нужны: EM-Marine Wiegand 26/34, сдвиг 5→3.3 В на D0/D1 (GPIO 15/16), оптореле OPEN (40), лампы 12 В (41/33), датчики ENTRY/EXIT (3/4), E-STOP (6).

| Сценарий | Ожидание |
|---|---|
| Пустой ACL, любая карта | DENY, красная лампа, нет пульса 40, событие Denied |
| ADMIN add UID, затем та же карта | GRANT, зелёная, пульс OPEN, имя/UID на OLED |
| Неизвестная карта | DENY |
| OPERATOR `ACCESS_OPEN` с консоли | REMOTE, dual ACK |
| VIEWER OPEN | `NO_PERMISSION` |
| E-STOP на GND | экран АВАРИЯ |

**Физический свайп на стенде в каскаде не делали.** DENY доказан пустым ACL + native. GRANT на реальном считывателе — ещё не закрыт.

### Можно тестировать Болид (когда будет прибор на шине)

**Path A (предпочтительный):** Orion-приборы ↔ **С2000-ПП** (официальный Modbus) ↔ isolator ↔ Heltec. `CONFIG_SET protocol BOLID_C2000PP`. Probe FC 0x03. `online: true` только после CRC-valid ответа.

CONTROL реле PP-001: только если включили allow-list (`-D MESH485_ALLOW_PP_RELAY=1` для R1). Иначе нет TX. Fire-disable / disarm-all **всегда отказ**.

**Path B MONITOR:** Сигнал-10 / Сигнал-20 на Orion 9600. `CONFIG_SET` `BOLID_SIGNAL10` или `BOLID_SIGNAL20` (persist `/m485_cfg.bin`). Нет native CONTROL реле. CRC OK ≠ контакт реле.

**С2000-М:** не второй мастер на той же шине. Предпочтительно Path A (С2000-ПП). Альтернатива — изолированный listen-only тап (DE = RX). RS-232 панели **не** на GPIO. Журнал — `EVENT_LIST`, не личное сообщение MeshCore. Проводка: [`c2000m.md`](c2000m.md). Прогон адаптера: [`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md) после 23.

Панель должна жить, если Heltec выключить.

---

## Какое оборудование нужно

Минимум для «узел живой + консоль»:

| Что | Зачем |
|---|---|
| Heltec WiFi LoRa 32 **V4 OLED 16 MB** | Единственная целевая плата |
| Антенна LoRa (регион EU868 / RU864 / …) | До любого TX в эфир |
| USB-C кабель | Прошивка, Windows-консоль, meshcore-cli |
| ПК Windows / Android | Flutter; Windows = USB CDC (раньше COM23) |

Турникет (Access):

| Что | Зачем |
|---|---|
| EM-Marine 125 кГц Wiegand 26/34 | Считыватель; Heltec **не** демодулирует 125 кГц |
| Сдвиг 5 В → 3.3 В на D0/D1 | Иначе сожжёте GPIO |
| Оптореле 1 канал | GPIO 40 → OPEN контроллера турникета |
| MOSFET ×2 + лампы 12 В | Зелёная 41 / красная 33 |
| БП 12 В (считыватель/лампы/турникет) + 5 В Heltec | Не кормить шину/мотор с 5 В Heltec |
| Сухие контакты ENTRY/EXIT, кнопка E-STOP | GPIO 3 / 4 / 6 на GND |

RS-485 / пожарка:

| Что | Зачем |
|---|---|
| **Изолированный** RS-485 (ISO3082-класс) | Не голый MAX485 на живой Orion |
| USB-RS485 dongle | Петля на столе без панели |
| С2000-ПП (желательно) | Официальный Path A Modbus |
| Сигнал-10 и/или Сигнал-20 | Только MONITOR, отдельный `CONFIG_SET protocol` |
| С2000-М (если нет ПП) | Listen-only на Orion A/B; не RS-232 на пины; события → EventStore |

Не ставить: GPS-модуль на GNSS_TX/RX (это UART1 RS-485). Не вешать Access на GPIO 19/20. Второй RS-485 — отдельный расширитель, не USB-пины.

Полный список: [`hardware/bill-of-materials.md`](../hardware/bill-of-materials.md), проводка [`hardware/wiring.md`](../hardware/wiring.md).

---

## Возможности vs границы (честно)

**Умеет**

- Быть беспроводным RS-485 портом + турникетом на одном Heltec
- Консоль оператора (объекты, приборы, события, Access, здоровье меша) без мессенджера
- Офлайн ACL турникета, если сервер/меш умер
- Честный dual ACK и честный TIMEOUT на пустой шине
- Не дать VIEWER открыть турникет и не дать выключить пожарку одной командой
- Persist списка операторов; optional bind pubkey (equality, не подпись MeshCore)

**Не умеет и не обещает**

- Сертифицированная пожарка / замена С2000
- AUTO discovery протокола шины
- Native CONTROL реле Сигнал-10/20
- OTA / подпись прошивки
- Фото по LoRa / на OLED / в JSON
- Чаты, контакты, QR MeshCore
- Postgres CRUD сотрудников
- Persist baud — ребут UART по-прежнему 9600; protocol переживает ребут (`/m485_cfg.bin`)
- Крипто EM-Marine; шифрование SPIFFS
- Подпись MESH 485 пакетов: JSON-`pubkey` на USB подделывается
- Factory имена `admin`/`operator`/`viewer` публичны, пока ADMIN не сузит список / не bind-нет ключ

---

## Следующий осмысленный шаг в коде

[`24-c2000m-monitor.md`](../agent/prompts/24-c2000m-monitor.md) — listen-only С2000-М → MESH 485 события (не DM чата). Проводка: [`c2000m.md`](c2000m.md).

Параллельно на стенде (не вместо 24): живой mux `ACCESS_ADD_CARD` + `ACCESS_GET_USERS` на COM23; физический свайп Wiegand; `CONFIG_SET` + ребут узла.

Не начинать без отдельного решения: AUTO, dual UART, OTA, Postgres/фото, native Orion CONTROL, MeshCore chat.
