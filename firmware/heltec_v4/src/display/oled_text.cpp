#include "display/oled_text.h"

#ifndef ACCESS_DISPLAY_SERIAL_ONLY
#include "display/font6x8_cyr.h"

#include <Adafruit_GFX.h>
#ifdef HELTEC_LORA_V4_TFT
#include <Adafruit_ST7789.h>
#define SSD1306_WHITE ST77XX_WHITE
#else
#define SSD1306_WHITE 1
#endif

static int cyrIndex(uint32_t cp) {
  if (cp >= 0x430 && cp <= 0x44F) cp -= 0x20;  // а–я → А–Я
  if (cp == 0x451) cp = 0x401;                 // ё → Ё
  if (cp == 0x401) return 6;
  if (cp >= 0x410 && cp <= 0x415) return (int)(cp - 0x410);
  if (cp >= 0x416 && cp <= 0x42F) return (int)(cp - 0x416) + 7;
  return -1;
}

static const char* nextCp(const char* s, uint32_t* cp) {
  const uint8_t b0 = (uint8_t)*s;
  if (b0 < 0x80) {
    *cp = b0;
    return s + 1;
  }
  if ((b0 & 0xE0) == 0xC0 && s[1]) {
    *cp = ((uint32_t)(b0 & 0x1F) << 6) | (uint8_t)(s[1] & 0x3F);
    return s + 2;
  }
  if ((b0 & 0xF0) == 0xE0 && s[1] && s[2]) {
    *cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) |
          (uint8_t)(s[2] & 0x3F);
    return s + 3;
  }
  *cp = b0;
  return s + 1;
}

static void drawCyr(Adafruit_GFX& d, int16_t x, int16_t y, int idx, uint16_t ink) {
  if (idx < 0 || idx >= (int)(sizeof(kOledCyrUpper) / sizeof(kOledCyrUpper[0]))) return;
  for (int col = 0; col < 5; ++col) {
    const uint8_t bits = kOledCyrUpper[idx][col];
    for (int row = 0; row < 7; ++row) {
      if (bits & (1u << row)) d.drawPixel(x + col, y + row, ink);
    }
  }
}

static void drawCyrScale(Adafruit_GFX& d, int16_t x, int16_t y, int idx, uint8_t scale) {
  if (idx < 0 || idx >= (int)(sizeof(kOledCyrUpper) / sizeof(kOledCyrUpper[0]))) return;
  if (scale < 1) scale = 1;
  for (int col = 0; col < 5; ++col) {
    const uint8_t bits = kOledCyrUpper[idx][col];
    for (int row = 0; row < 7; ++row) {
      if ((bits & (1u << row)) == 0) continue;
      d.fillRect(x + col * scale, y + row * scale, scale, scale, SSD1306_WHITE);
    }
  }
}

void oledPrintUtf8Bold(Adafruit_GFX& d, int16_t x, int16_t y, const char* utf8, uint8_t scale) {
  if (!utf8) return;
  if (scale < 1) scale = 1;
  const char* s = utf8;
  int16_t cx = x;
  while (*s) {
    uint32_t cp = 0;
    s = nextCp(s, &cp);
    const int idx = cyrIndex(cp);
    if (idx >= 0) {
      drawCyrScale(d, cx, y, idx, scale);
      drawCyrScale(d, (int16_t)(cx + 1), y, idx, scale);
      cx = (int16_t)(cx + 6 * scale);
    } else if (cp < 0x80) {
      d.setTextSize(scale);
      d.setCursor(cx, y);
      d.write((uint8_t)cp);
      d.setTextSize(scale);
      d.setCursor((int16_t)(cx + 1), y);
      d.write((uint8_t)cp);
      cx = (int16_t)(cx + 6 * scale);
    }
  }
  d.setTextSize(1);
}

void oledPrintUtf8(Adafruit_GFX& d, int16_t x, int16_t y, const char* utf8, bool black) {
  if (!utf8) return;
#ifdef HELTEC_LORA_V4_TFT
  const uint16_t ink = black ? ST77XX_BLACK : ST77XX_WHITE;
#else
  const uint16_t ink = black ? 0 : SSD1306_WHITE;
#endif
  d.setTextColor(ink);
  const char* s = utf8;
  int16_t cx = x;
  int16_t cy = y;
  while (*s) {
    uint32_t cp = 0;
    s = nextCp(s, &cp);
    if (cp == '\n') {
      cy = (int16_t)(cy + 8);
      cx = x;
      continue;
    }
    const int idx = cyrIndex(cp);
    if (idx >= 0) {
      drawCyr(d, cx, cy, idx, ink);
      cx = (int16_t)(cx + 6);
    } else if (cp < 0x80) {
      d.setCursor(cx, cy);
      d.write((uint8_t)cp);
      cx = d.getCursorX();
    }
  }
  d.setTextColor(SSD1306_WHITE);
}
#endif
