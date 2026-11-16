#pragma once

#include <stddef.h>
#include <stdint.h>

// RDM6300 9600 8N1 frame: 0x02, 10 hex chars (5 bytes), 2 hex chars (XOR), 0x03.
// A bad frame bumps the error counter and is not a card.
// The same UID inside kRepeatWindowMs is one read, not a stream.
class Rdm6300Parser {
public:
  static constexpr uint32_t kRepeatWindowMs = 2000;

  void push(const uint8_t* data, size_t len, uint32_t now_ms);
  bool take(char* uid, size_t max);
  uint32_t errorCount() const { return errors_; }

private:
  uint8_t buf_[12] = {};
  uint8_t got_ = 0;
  bool in_frame_ = false;
  bool pending_ = false;
  uint32_t errors_ = 0;
  uint32_t last_ms_ = 0;
  bool have_last_ = false;
  char last_uid_[16] = {};
  char pending_uid_[16] = {};

  void finish(uint32_t now_ms);
  void fail();
};
