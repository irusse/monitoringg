#pragma once

#include <Arduino.h>

// Frozen Heltec WiFi LoRa 32 V4 Access pinout (FIRST_RUN / Phase 01).
// GPIO 19/20 = ESP32-S3 USB PHY. Never Wiegand, sensors, relay, or lamps.
// Do not reuse LoRa / PA / OLED pins for Access I/O.

// --- Wiegand (level-shifted 5 V → 3.3 V) ---
#ifndef PIN_WIEGAND_D0
#define PIN_WIEGAND_D0 15
#endif
#ifndef PIN_WIEGAND_D1
#define PIN_WIEGAND_D1 16
#endif

// --- Turnstile ---
#ifndef PIN_RELAY_OPEN
#define PIN_RELAY_OPEN 40
#endif
#ifndef PIN_LAMP_GREEN
#define PIN_LAMP_GREEN 41
#endif
#ifndef PIN_LAMP_RED
#define PIN_LAMP_RED 33
#endif
#ifndef PIN_SENSOR_ENTRY
#define PIN_SENSOR_ENTRY 3
#endif
#ifndef PIN_SENSOR_EXIT
#define PIN_SENSOR_EXIT 4
#endif
#ifndef PIN_EMERGENCY
#define PIN_EMERGENCY 6
#endif

// --- USER / BOOT (Access menu, not MeshCore UI) ---
#ifndef PIN_USER_BTN
#define PIN_USER_BTN 0
#endif
#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

// --- SSD1306 OLED (Heltec V4 silk: SDA_OLED / SCL_OLED / RST_OLED) ---
// Arduino Wire defaults on this variant are SDA=3 SCL=4 (env/GPS). Access
// ENTRY/EXIT occupy 3/4, so always Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL).
#ifndef PIN_OLED_SDA
#define PIN_OLED_SDA 17
#endif
#ifndef PIN_OLED_SCL
#define PIN_OLED_SCL 18
#endif
#ifndef PIN_OLED_RST
#define PIN_OLED_RST 21
#endif
#ifndef PIN_OLED_RESET
#define PIN_OLED_RESET PIN_OLED_RST
#endif

// Peripheral / OLED rail. Heltec V4 Vext is a PMOS on GPIO 36: LOW = ON.
// MeshCore -D PIN_VEXT_EN_ACTIVE=HIGH is inverted vs the hardware; companion
// never claims periph_power, so HeltecV4Board::begin() leaves the pin LOW
// (OLED on). Access must not digitalWrite HIGH — that floats the rail at
// ~1.6 V, blanks the SSD1306, and can hang I2C so USER never runs.
#ifndef PIN_VEXT_EN
#define PIN_VEXT_EN 36
#endif
#ifndef PIN_VEXT_EN_ACTIVE
#define PIN_VEXT_EN_ACTIVE HIGH
#endif
#ifndef PIN_VEXT_OLED_ON
#define PIN_VEXT_OLED_ON LOW
#endif

// USB PHY — documented so they are never reassigned
#define PIN_USB_DM 19
#define PIN_USB_DP 20

// --- Isolated RS-485 UART1 (Phase 05 frozen). GNSS module cannot share these. ---
// Silk GNSS_TX / GNSS_RX. MeshCore ENV_INCLUDE_GPS must stay 0.
#ifndef PIN_RS485_TX
#define PIN_RS485_TX 39
#endif
#ifndef PIN_RS485_RX
#define PIN_RS485_RX 38
#endif
#ifndef PIN_RS485_DE
#define PIN_RS485_DE 26
#endif
#ifndef PIN_RS485_DE_TX
#define PIN_RS485_DE_TX HIGH
#endif
#ifndef PIN_RS485_DE_RX
#define PIN_RS485_DE_RX LOW
#endif

// --- PCN «Тревога» (source role). Not RS-485, not USB 19/20. ---
// INPUT_PULLUP, active LOW. Either pin closed is one alarm for this node.
// GPIO 45 is an ESP32-S3 strap pin: keep it high at reset (external 10 kΩ to 3.3 V).
// A 12 V PCN output must go through an optocoupler, never onto the pad.
#ifndef PIN_PCN_ALARM
#define PIN_PCN_ALARM 47
#endif
#ifndef PIN_PCN_ALARM_B
#define PIN_PCN_ALARM_B 45
#endif

// Vibration motor. GPIO cannot feed the motor coil: 3.3 V -> motor ->
// NPN collector, emitter to GND, GPIO 48 through 1 kΩ to the base.
// HIGH = run. Same pad on V4 and V3 (spare next to GPIO 47).
#ifndef PIN_VIBE
#define PIN_VIBE 48
#endif
#ifndef PIN_VIBE_ON
#define PIN_VIBE_ON HIGH
#endif

// On-board LED (Heltec V3/V4 GPIO 35). HIGH = lit. Alarm blink is 2 Hz.
#ifndef PIN_ALARM_LED
#define PIN_ALARM_LED 35
#endif

// Receiver alarm lamp. GPIO is 3.3 V, 2 Hz, HIGH = on, only while an alarm
// is showing. A 5 V bulb goes through an NPN: GPIO 42 — 1 kΩ — base,
// emitter to GND, lamp from +5 V to the collector. Do not pinMode at boot.
#ifndef PIN_ALARM_LAMP
#define PIN_ALARM_LAMP 42
#endif
