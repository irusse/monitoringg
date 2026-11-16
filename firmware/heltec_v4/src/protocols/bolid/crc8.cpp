#include "protocols/bolid/crc8.h"

// CRC-8/MAXIM-DOW (Dallas 1-Wire): poly 0x31, reflected 0x8C, init 0, xor-out 0.
// Matches the table published with Orion dumps (Habr 563584 / CXEM topic 105126).
static uint8_t crc8_dallas_maxim(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  if (!data || len == 0) return 0;
  for (size_t i = 0; i < len; ++i) {
    uint8_t b = data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint8_t mix = (uint8_t)((crc ^ b) & 1u);
      crc >>= 1;
      if (mix) crc ^= 0x8Cu;
      b >>= 1;
    }
  }
  return crc;
}

// Naive CRC-8 (CRC-8/SMBUS-like): poly 0x07, not reflected, init 0, xor-out 0.
static uint8_t crc8_generic(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  if (!data || len == 0) return 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x80u) {
        crc = (uint8_t)((crc << 1) ^ 0x07u);
      } else {
        crc = (uint8_t)(crc << 1);
      }
    }
  }
  return crc;
}

uint8_t crc8(Crc8Algo algo, const uint8_t* data, size_t len) {
  switch (algo) {
    case Crc8Algo::Orion:
    case Crc8Algo::DallasMaxim:
      return crc8_dallas_maxim(data, len);
    case Crc8Algo::Generic:
      return crc8_generic(data, len);
    case Crc8Algo::None:
    default:
      return 0;
  }
}

uint8_t crc8_orion(const uint8_t* data, size_t len) {
  return crc8(Crc8Algo::Orion, data, len);
}
