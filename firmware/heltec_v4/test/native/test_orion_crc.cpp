#include "orion_vectors.h"
#include "protocols/bolid/crc8.h"

#include <stdio.h>

extern int g_fail;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

int run_orion_crc_tests() {
  CHECK(crc8_orion(nullptr, 0) == 0, "orion empty null");
  const uint8_t empty_byte = 0;
  CHECK(crc8_orion(&empty_byte, 0) == 0, "orion empty len");
  CHECK(crc8(Crc8Algo::DallasMaxim, nullptr, 0) == 0, "dallas empty");
  CHECK(crc8(Crc8Algo::Generic, nullptr, 0) == 0, "generic empty");
  CHECK(crc8(Crc8Algo::None, &empty_byte, 1) == 0, "none ignores data");

  const uint8_t one = 0x01;
  CHECK(crc8_orion(&one, 1) == 0x5E, "orion 1-byte 0x01");
  CHECK(crc8(Crc8Algo::DallasMaxim, &one, 1) == 0x5E, "dallas 1-byte 0x01");
  CHECK(crc8(Crc8Algo::Generic, &one, 1) != 0x5E, "generic 1-byte mismatches dallas");

  static const uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK(crc8_orion(kCheck, sizeof(kCheck)) == 0xA1, "CRC-8/MAXIM check 123456789");

  for (size_t v = 0; v < kOrionCrcVectorCount; ++v) {
    const OrionCrcVector& vec = kOrionCrcVectors[v];
    CHECK(vec.len >= 2, "vector min length");
    const uint8_t got =
        crc8_orion(vec.frame, vec.len - 1);
    const uint8_t dallas =
        crc8(Crc8Algo::DallasMaxim, vec.frame, vec.len - 1);
    const uint8_t generic =
        crc8(Crc8Algo::Generic, vec.frame, vec.len - 1);
    if (got != vec.crc) {
      printf("FAIL: %s orion crc got %02X want %02X\n", vec.id, got, vec.crc);
      ++g_fail;
    }
    if (dallas != vec.crc) {
      printf("FAIL: %s dallas crc got %02X want %02X\n", vec.id, dallas, vec.crc);
      ++g_fail;
    }
    if (generic == vec.crc) {
      printf("FAIL: %s generic unexpectedly matched %02X\n", vec.id, generic);
      ++g_fail;
    }
    const uint8_t residue = crc8_orion(vec.frame, vec.len);
    if (residue != 0) {
      printf("FAIL: %s residue %02X want 00\n", vec.id, residue);
      ++g_fail;
    }
  }

  uint8_t flipped[sizeof(kOrionHabrKey)];
  for (size_t i = 0; i < sizeof(kOrionHabrKey); ++i) flipped[i] = kOrionHabrKey[i];
  flipped[0] ^= 0x01;
  const uint8_t bad = crc8_orion(flipped, sizeof(flipped) - 1);
  CHECK(bad != kOrionHabrKey[sizeof(kOrionHabrKey) - 1], "flipped bit mismatches");
  CHECK(crc8_orion(flipped, sizeof(flipped)) != 0, "flipped residue nonzero");

  CHECK(crc8(Crc8Algo::Orion, kOrionHabrKey, sizeof(kOrionHabrKey) - 1) ==
            crc8_orion(kOrionHabrKey, sizeof(kOrionHabrKey) - 1),
        "crc8 Orion == crc8_orion");

  return 0;
}
