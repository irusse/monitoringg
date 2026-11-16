#pragma once

#include <stddef.h>
#include <stdint.h>

// Published bus-frame fixtures. No decrypted vendor firmware.
// Sources: Habr 563584 (worked hex that cites CXEM topic 105126, started 2012-03-21).
// CRC is the last byte. Compute crc8_orion over bytes [0 .. len-2].

struct OrionCrcVector {
  const char* id;
  const char* source;
  const uint8_t* frame;
  size_t len;
  uint8_t crc;
};

static const uint8_t kOrionHabrKey[] = {
    0x03, 0x06, 0x00, 0x11, 0xBA, 0xBA, 0x8D};
static const uint8_t kOrionHabrStatusReq[] = {
    0x83, 0x08, 0x00, 0xED, 0xB8, 0xBA, 0xBA, 0xBA, 0x62};
static const uint8_t kOrionHabrStatusRsp[] = {
    0x83, 0x0A, 0xE2, 0xB8, 0xBA, 0xBE, 0xB9, 0x7D, 0x2F, 0x72, 0xD7};

static const OrionCrcVector kOrionCrcVectors[] = {
    {"habr_set_global_key",
     "https://habr.com/ru/articles/563584/ (cites CXEM topic 105126)",
     kOrionHabrKey, sizeof(kOrionHabrKey), 0x8D},
    {"habr_status_req_encrypted",
     "https://habr.com/ru/articles/563584/ (cites CXEM topic 105126)",
     kOrionHabrStatusReq, sizeof(kOrionHabrStatusReq), 0x62},
    {"habr_status_rsp_encrypted",
     "https://habr.com/ru/articles/563584/ (cites CXEM topic 105126)",
     kOrionHabrStatusRsp, sizeof(kOrionHabrStatusRsp), 0xD7},
};

static const size_t kOrionCrcVectorCount =
    sizeof(kOrionCrcVectors) / sizeof(kOrionCrcVectors[0]);

int run_orion_crc_tests();
