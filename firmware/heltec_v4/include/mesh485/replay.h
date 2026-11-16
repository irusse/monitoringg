#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef MESH485_REPLAY_SLOTS
#define MESH485_REPLAY_SLOTS 16
#endif
#ifndef MESH485_REPLAY_TTL_S
#define MESH485_REPLAY_TTL_S 30
#endif
#ifndef MESH485_REPLAY_TS_WINDOW_S
#define MESH485_REPLAY_TS_WINDOW_S 120
#endif

class Mesh485Replay {
public:
  // now_s: monotonic seconds (millis/1000). unix_now: 0 skips wall-clock ts check.
  bool isReplay(const char* id, const char* optr, int64_t ts, uint32_t now_s,
                int64_t unix_now);
  void remember(const char* id, const char* optr, int64_t ts, uint32_t now_s);
  void clear();

private:
  struct Slot {
    char id[12];
    char optr[24];
    uint32_t seen_s;
    int64_t ts;
    bool used;
  };
  Slot slots_[MESH485_REPLAY_SLOTS] = {};
  uint8_t next_ = 0;
};
