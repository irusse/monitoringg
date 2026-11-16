#pragma once

#include <Arduino.h>
#include <stdint.h>

struct CardEvent {
  char uid[24];
  uint8_t bit_count;
  uint32_t timestamp_ms;
};

class WiegandReader {
public:
  void begin(int d0, int d1);
  bool poll(CardEvent& out);

private:
  int d0_ = -1;
  int d1_ = -1;
};
