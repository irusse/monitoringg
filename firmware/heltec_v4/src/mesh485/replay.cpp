#include "mesh485/replay.h"

#include <string.h>

static bool sameZ(const char* a, const char* b) {
  if (!a) a = "";
  if (!b) b = "";
  return strcmp(a, b) == 0;
}

static void copyZ(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

void Mesh485Replay::clear() {
  memset(slots_, 0, sizeof(slots_));
  next_ = 0;
}

bool Mesh485Replay::isReplay(const char* id, const char* optr, int64_t ts, uint32_t now_s,
                             int64_t unix_now) {
  if (!id || !id[0]) return false;
  if (unix_now > 0 && ts > 0) {
    const int64_t age = unix_now - ts;
    if (age > (int64_t)MESH485_REPLAY_TS_WINDOW_S || age < -(int64_t)MESH485_REPLAY_TS_WINDOW_S) {
      return true;
    }
  }
  for (uint8_t i = 0; i < MESH485_REPLAY_SLOTS; ++i) {
    if (!slots_[i].used) continue;
    if ((now_s - slots_[i].seen_s) > MESH485_REPLAY_TTL_S) continue;
    if (sameZ(slots_[i].id, id) && sameZ(slots_[i].optr, optr)) return true;
  }
  return false;
}

void Mesh485Replay::remember(const char* id, const char* optr, int64_t ts, uint32_t now_s) {
  Slot& s = slots_[next_];
  next_ = (uint8_t)((next_ + 1) % MESH485_REPLAY_SLOTS);
  s.used = true;
  s.seen_s = now_s;
  s.ts = ts;
  copyZ(s.id, sizeof(s.id), id);
  copyZ(s.optr, sizeof(s.optr), optr);
}
