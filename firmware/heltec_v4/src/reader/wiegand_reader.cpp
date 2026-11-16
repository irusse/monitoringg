#include "reader/wiegand_reader.h"
#include "access/pins.h"
#include <string.h>
#include <stdio.h>
#include <WiegandNG.h>

// WiegandNG from vendor/esp32-rfid: 58-bit buffer, 25 ms packet gap.
static constexpr unsigned kWiegandBits = 58;
static constexpr unsigned kPacketGapMs = 25;
static constexpr unsigned kMinPiccBits = 24;
static constexpr unsigned kMaxPiccBits = 58;

static WiegandNG wg;
static bool wg_ok = false;

static void bitsToUid(const String& bits, char* uid, size_t n) {
  uint64_t v = 0;
  const unsigned len = bits.length();
  for (unsigned i = 0; i < len; i++) {
    v <<= 1;
    if (bits[i] == '1') v |= 1ULL;
  }
  snprintf(uid, n, "%llX", (unsigned long long)v);
}

void WiegandReader::begin(int d0, int d1) {
  d0_ = d0;
  d1_ = d1;
#ifndef ACCESS_STUB_NO_RADIO
  wg_ok = wg.begin((uint8_t)d0_, (uint8_t)d1_, kWiegandBits, kPacketGapMs);
#else
  (void)d0;
  (void)d1;
  wg_ok = false;
#endif
}

bool WiegandReader::poll(CardEvent& out) {
  memset(&out, 0, sizeof(out));
  if (!wg_ok) return false;
  if (!wg.available()) return false;

  wg.pause();
  const unsigned bits = wg.getBitCounted();
  if (bits < kMinPiccBits || bits > kMaxPiccBits) {
    wg.clear();
    return false;
  }

  bitsToUid(wg.getRawData(), out.uid, sizeof(out.uid));
  if (out.uid[0] == 0) {
    strncpy(out.uid, "0", sizeof(out.uid) - 1);
  }
  out.bit_count = (uint8_t)bits;
  out.timestamp_ms = millis();
  wg.clear();
  return true;
}
