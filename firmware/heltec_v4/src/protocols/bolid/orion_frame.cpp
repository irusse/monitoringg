#include "protocols/bolid/orion_frame.h"

#include "protocols/bolid/crc8.h"

// Encrypted Orion: ADDR bit7 set, byte 2 = GLOBAL^MESSAGE (0 when equal),
// command/payload XOR MESSAGE_KEY. CRC-8/MAXIM on the prefix (orion-frames.md).
size_t orion_build_status_request(uint8_t* out, size_t max, uint8_t addr) {
  if (!out || max < kOrionStatusReqLen) return 0;
  if (addr < 1 || addr > 127) addr = kOrionDefaultAddr;
  out[0] = (uint8_t)(addr | 0x80u);
  out[1] = 0x08;  // length = 9 - 1 (dump habr_status_req_encrypted)
  out[2] = 0x00;
  out[3] = (uint8_t)(kOrionCmdStatus ^ kOrionMessageKey);     // 0xED
  out[4] = (uint8_t)(kOrionCmdStatusArg ^ kOrionMessageKey);  // 0xB8
  out[5] = kOrionMessageKey;
  out[6] = kOrionMessageKey;
  out[7] = kOrionMessageKey;
  out[8] = crc8_orion(out, 8);
  return kOrionStatusReqLen;
}

bool orion_frame_crc_ok(const uint8_t* frame, size_t len) {
  if (!frame || len < 3) return false;
  if ((size_t)frame[1] + 1u != len) return false;
  return crc8_orion(frame, len - 1) == frame[len - 1];
}

uint8_t orion_frame_peer(const uint8_t* frame) {
  return frame ? (uint8_t)(frame[0] & 0x7Fu) : 0;
}
