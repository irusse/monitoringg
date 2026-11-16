#pragma once

#include <stddef.h>
#include <stdint.h>

// Mixed UTF-8 print for SSD1306: ASCII uses Adafruit 6×8, Cyrillic uses
// font6x8_cyr.h (А–Я / а–я / Ёё). No FreeSans.
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
class Adafruit_GFX;
void oledPrintUtf8(Adafruit_GFX& d, int16_t x, int16_t y, const char* utf8, bool black = false);
// scale 2 draws each glyph twice as wide and tall, then again one pixel to the right.
void oledPrintUtf8Bold(Adafruit_GFX& d, int16_t x, int16_t y, const char* utf8, uint8_t scale);
#endif
