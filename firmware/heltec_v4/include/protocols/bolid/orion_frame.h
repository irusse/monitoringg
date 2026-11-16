#pragma once

#include <stddef.h>
#include <stdint.h>

// Shared Orion encrypted status-read (Habr 563584 cmd 0x57/0x02, MESSAGE_KEY 0xBA).
// Used by Signal-10 and Signal-20 MONITOR. CRC-8/MAXIM via crc8_orion.
// Not wired to RS485_TRANSACTION (opaque RAW).

static constexpr uint8_t kOrionDefaultAddr = 1;
static constexpr uint8_t kOrionDumpAddr = 3;  // Habr fixture address
static constexpr uint8_t kOrionMessageKey = 0xBA;
static constexpr uint8_t kOrionCmdStatus = 0x57;
static constexpr uint8_t kOrionCmdStatusArg = 0x02;
static constexpr size_t kOrionStatusReqLen = 9;

size_t orion_build_status_request(uint8_t* out, size_t max, uint8_t addr);
bool orion_frame_crc_ok(const uint8_t* frame, size_t len);
uint8_t orion_frame_peer(const uint8_t* frame);
