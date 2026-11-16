# Heltec V4 OLED — Access UI

Display: SSD1306 **128×64**, I2C SDA=17 SCL=18 RST=21 (`HELTEC_LORA_V4_OLED`). Driver: MeshCore `SSD1306Display` / Adafruit_SSD1306. Do **not** show MeshCore companion/repeater/chat screens.

Font: Adafruit 6×8 for ASCII (ids, `M485 9600`, `BR RO`) plus a 5×7/6×8 Cyrillic table (`include/display/font6x8_cyr.h`, `oledPrintUtf8`) covering `А–Яа–яЁё`. Not FreeSans. Operator chrome is **Russian**. Protocol tokens stay Latin.

Indicator top-right: `M●` radio on, `M○` off, `M!` error. Repeater on: small `R` next to it.

## 1. Boot / idle (main)

```
TS-001            M●
ГОТОВ
ПРИЛОЖИТЕ КАРТУ
НОРМА
or ТРЕВОГА
PP-001 FAULT
or S10-001 FAULT
or S20-001 FAULT
M485 9600
```

READY overlay: `PP-001` plus `NORMAL` / `FAULT` / `ALARM` (empty bus / offline → `FAULT`), or `S10-001 FAULT` / `NORMAL` when CONFIG protocol is `BOLID_SIGNAL10`, or `S20-001 FAULT` / `NORMAL` when it is `BOLID_SIGNAL20`. The row under the card prompt is the last known PCN state: the local GPIO 47 level, or the state accepted from the other node. `НОРМА` while that state is open, `ТРЕВОГА` while it is alarm. Optional pending-queue hint: `ГОТОВ Q:n` when events wait for mesh ACK. User count stays on the INFO screen. RS-485 baud/bridge line is unchanged (`M485 9600` / `RS485 N B` / `BR RO`).

Long-press USER (GPIO 0, >1.5 s) → menu. Short press cycles idle extras (uptime) if needed.

## 2. Access granted (~3 s, then idle)

```
ДОСТУП ОК         M●
Ivan Petrov
ENTRY  12:31:42
```

## 3. Access denied (~3 s)

```
ОТКАЗ             M●
НЕИЗВ.КАРТА
12:32:04
```

## 4. Remote open (BLE/USB/LoRa)

Triggered by MESH 485 `ACCESS_OPEN` (OPERATOR+): firmware calls `showRemote` then the existing granted/idle path. VIEWER is refused (`NO_PERMISSION`) and does not change the panel.

```
ДИСТАНЦ.          M●
ADMIN
ВЫПОЛН...
```

Then granted or idle.

## 5. Locked / emergency

```
БЛОКИРОВКА        M●
or
АВАРИЯ            M!
СТОП
```

## 6. Menu (long-press USER, >1.5 s) — Access only, not MeshCore chat

```
МЕНЮ              M● R
> ИНФО
  ПОВТОР ВКЛ
  НАЗАД
```

Short press moves the cursor. Long press selects: Info, toggle repeater (local, persisted), or Back to idle. Repeater off: `ПОВТОР ВЫКЛ`.

Info: device id, **users** = enabled ACL entries (not a fake number), last UID, heap, uptime. Repeater identity/radio still via MeshCore tools.

Default 6×8 font has no Unicode ●/○: draw `M` plus a filled (on) or empty (off) circle, or `M!` on error. Repeater on: `R` at the far right.

Demo (no RFID yet): short press on idle cycles GRANTED → DENIED → REMOTE → LOCKED → EMERGENCY → READY.

READY also shows `PP-001 FAULT` (or `NORMAL` / `ALARM` after a CRC-valid Modbus read), `S10-001 FAULT` / `NORMAL` when the selected CONFIG protocol is `BOLID_SIGNAL10`, or `S20-001 FAULT` / `NORMAL` when it is `BOLID_SIGNAL20`, plus `M485 9600` (or `RS485 N B` after idle UART1 RX; `BR RO` / `BR RAW` / `BR CTL` while a bridge session is open). Registered adapter count is in MESH 485 `NODE_STATUS.devices`, not this row (keeps baud/bridge visible). After **10 s** with no button and no access event the SSD1306 is commanded **DISPLAYOFF** (VEXT stays LOW / rail on). USER press wakes the panel; that press is not a menu/demo. Emergency stays on. Mesh status updates do not reset the timer.

## 7. Bridge session

```
TS-001            M●
МОСТ
READ_ONLY
USER стоп
```

Shown while `BRIDGE_OPEN` has a RAM session. USER **long-press** aborts the session → MONITOR / READY (DE RX). Emergency still wins. The panel **stays awake** for the whole session (including RAW); abort or idle-timeout close returns to READY, which can blank after 10 s. Wake on open and on abort. No BLE PIN, no MeshCore chat.

No QR, no photos, no employee photo on 64 px row.
