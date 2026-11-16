#include "reader/rdm6300.h"

#include <string.h>

static int hex_nibble(uint8_t c) {
  if (c >= '0' && c <= '9') return (int)(c - '0');
  if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
  return -1;
}

void Rdm6300Parser::fail() {
  if (errors_ < 0xFFFFFFFFu) ++errors_;
  in_frame_ = false;
  got_ = 0;
}

void Rdm6300Parser::finish(uint32_t now_ms) {
  in_frame_ = false;
  if (got_ != 12) {
    fail();
    return;
  }
  uint8_t raw[6];
  for (int i = 0; i < 6; ++i) {
    const int hi = hex_nibble(buf_[i * 2]);
    const int lo = hex_nibble(buf_[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      fail();
      return;
    }
    raw[i] = (uint8_t)((hi << 4) | lo);
  }
  uint8_t xor_sum = 0;
  for (int i = 0; i < 5; ++i) xor_sum = (uint8_t)(xor_sum ^ raw[i]);
  if (xor_sum != raw[5]) {
    fail();
    return;
  }
  char uid[11];
  for (int i = 0; i < 10; ++i) {
    char c = (char)buf_[i];
    if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
    uid[i] = c;
  }
  uid[10] = 0;
  if (have_last_ && strcmp(last_uid_, uid) == 0 &&
      (uint32_t)(now_ms - last_ms_) < kRepeatWindowMs) {
    got_ = 0;
    return;
  }
  memcpy(last_uid_, uid, sizeof(last_uid_));
  have_last_ = true;
  last_ms_ = now_ms;
  memcpy(pending_uid_, uid, sizeof(pending_uid_));
  pending_ = true;
  got_ = 0;
}

void Rdm6300Parser::push(const uint8_t* data, size_t len, uint32_t now_ms) {
  if (!data || len == 0) return;
  for (size_t i = 0; i < len; ++i) {
    const uint8_t b = data[i];
    if (!in_frame_) {
      if (b == 0x02) {
        in_frame_ = true;
        got_ = 0;
      }
      continue;
    }
    if (b == 0x02) {
      got_ = 0;
      continue;
    }
    if (b == 0x03) {
      finish(now_ms);
      continue;
    }
    if (got_ >= sizeof(buf_)) {
      fail();
      if (b == 0x02) {
        in_frame_ = true;
        got_ = 0;
      }
      continue;
    }
    buf_[got_++] = b;
  }
}

bool Rdm6300Parser::take(char* uid, size_t max) {
  if (!pending_ || !uid || max < 11) return false;
  pending_ = false;
  memcpy(uid, pending_uid_, 11);
  return true;
}
