# MESH 485 Extender

Универсальный RS-485 шлюз на Heltec V4: MeshCore (LoRa mesh) + изолированный RS-485 + адаптеры приборов. Болид С2000 / Сигнал-10 / Сигнал-20 — первые адаптеры. Турникет — профиль `PROFILE_ACCESS` на той же ноде, не отдельная прошивка.

```
MeshCore → MESH 485 Protocol → Device Abstraction → Adapter → RS-485
```

Не делаем «команды С2000 внутри MeshCore». Не делаем прошивку только для Болид.

## Что уже есть (фазы 01–04)

Прошивка `heltec_v4_access`: OLED Access UI, MeshCore companion BLE+USB, Wiegand, реле/лампы/датчики турникета. ACL пока заглушка (любая карта = проход).

Следующий шаг: изолированный RS-485 HAL — `agent/prompts/05-rs485-hal.md`.

## Два контура на одной ноде

| Профиль | Шина | Задача |
|---|---|---|
| `PROFILE_RS485` | изолированный A/B | MONITOR / BRIDGE / CONTROLLER для приборов |
| `PROFILE_ACCESS` | Wiegand + реле | офлайн турникет, события, 3MAP |

Режимы RS-485: **MONITOR** (по умолчанию) · **BRIDGE** (сервис, с ролью) · **CONTROLLER** (только объявленные capabilities).

Пожарная автоматика при потере LoRa **продолжает работать штатно**. Нода — мониторинг/интеграция, не единственный путь снятия с охраны.

## Берём готовое

- [MeshCore](https://github.com/meshcore-dev/MeshCore) — LoRa, companion BLE+USB, repeater
- [hass-hubc2000pp](https://github.com/Regressor/hass-hubc2000pp) — модель зон/реле/разделов/событий (Apache-2.0); **не** парсер Orion UART
- [ESP32-RFID](https://github.com/pvtex/esp32-rfid) — Wiegand, ACL, реле (не их GPIO)
- [PiDoors](https://github.com/sybethiesant/pidoors) — экраны СКУД и схема БД
- Официальные документы Болид / С2000-ПП — кадры, Modbus, тайминги

Каскад: [agent/CASCADE.md](agent/CASCADE.md). Архитектура: [docs/architecture.md](docs/architecture.md). Рынок аналогов: [docs/competitive.md](docs/competitive.md).
