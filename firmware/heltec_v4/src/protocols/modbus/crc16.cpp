#include "protocols/modbus/crc16.h"

uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  if (!data) return crc;
  for (size_t i = 0; i < len; ++i) {
    crc = (uint16_t)(crc ^ data[i]);
    for (uint8_t b = 0; b < 8; ++b) {
      if (crc & 1u) {
        crc = (uint16_t)((crc >> 1) ^ 0xA001u);
      } else {
        crc = (uint16_t)(crc >> 1);
      }
    }
  }
  return crc;
}

size_t crc16_modbus_append(uint8_t* buf, size_t len, size_t max) {
  if (!buf || len < 2 || len + 2 > max) return 0;
  const uint16_t crc = crc16_modbus(buf, len);
  buf[len] = (uint8_t)(crc & 0xFF);
  buf[len + 1] = (uint8_t)(crc >> 8);
  return len + 2;
}

bool crc16_modbus_ok(const uint8_t* data, size_t len) {
  if (!data || len < 4) return false;
  const uint16_t got =
      (uint16_t)((uint16_t)data[len - 2] | ((uint16_t)data[len - 1] << 8));
  return got == crc16_modbus(data, len - 2);
}
