#pragma once

#include <stddef.h>
#include <stdint.h>

// Orion CRC-8 (Path B). Wired to Signal-10 / Signal-20 MONITOR probes only.
// Not used by RS485_TRANSACTION (still opaque RAW).
enum class Crc8Algo : uint8_t {
  None = 0,
  Orion = 1,        // dumps matched — production default (Dallas/Maxim reflected)
  DallasMaxim = 2,  // compare only (same poly/init as Orion on current fixtures)
  Generic = 3       // compare only (poly 0x07, not reflected)
};

uint8_t crc8(Crc8Algo algo, const uint8_t* data, size_t len);

// Production helper: verified algo only (Crc8Algo::Orion).
uint8_t crc8_orion(const uint8_t* data, size_t len);
