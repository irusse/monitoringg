#pragma once

#include <stddef.h>
#include <stdint.h>

// Modbus-RTU CRC-16 (poly 0xA001 reflected / 0x8005). Not Orion crc8.
uint16_t crc16_modbus(const uint8_t* data, size_t len);

// Append CRC low-byte-first. Returns len+2, or 0 if it would not fit.
size_t crc16_modbus_append(uint8_t* buf, size_t len, size_t max);

// True if len >= 4 and the trailing CRC matches the PDU.
bool crc16_modbus_ok(const uint8_t* data, size_t len);
