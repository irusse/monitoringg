#pragma once

#include <stdint.h>

// Monotonic ms for adapter poll intervals. Arduino millis() on device;
// tests poke the native clock so poll() stays non-blocking.

#ifdef MESH485_NATIVE

inline uint32_t& mesh485_clock_ms() {
  static uint32_t t = 0;
  return t;
}

inline uint32_t mesh485_now_ms() { return mesh485_clock_ms(); }

inline void mesh485_set_now_ms(uint32_t ms) { mesh485_clock_ms() = ms; }

#else

#include <Arduino.h>

inline uint32_t mesh485_now_ms() { return millis(); }

#endif
