#pragma once

#include <stdint.h>

// Monitor (Heltec V3): a new alarm generation turns the panel on for this long.
// The same generation does not hold the screen. No alarm signal clears it at once.
static constexpr uint32_t kAlarmHoldMs = 40000;
static constexpr uint8_t kAlarmStrobeCount = 40;

struct AlarmUiState {
  uint16_t seen;
  uint32_t until;
  uint8_t showing;
};

struct AlarmUiStep {
  uint8_t start_strobe;
  uint8_t clear;
  uint8_t show;
};

// hold_ms == 0 latches until alarm is false. A non-zero hold expires on its own.
// Generation 0 matches the initial seen value, so a stuck level with no edge
// does not light the screen.
inline AlarmUiStep alarm_ui_step(AlarmUiState* st, int alarm, uint16_t gen, uint32_t now,
                                 uint32_t hold_ms) {
  AlarmUiStep out = {0, 0, 0};
  if (!st) return out;
  if (!alarm) {
    if (st->showing) out.clear = 1;
    st->showing = 0;
    st->until = 0;
    return out;
  }
  if (gen != st->seen) {
    st->seen = gen;
    st->showing = 1;
    st->until = hold_ms ? (now + hold_ms) : 0;
    out.start_strobe = 1;
    out.show = 1;
    return out;
  }
  if (st->showing && st->until != 0 && (int32_t)(now - st->until) >= 0) {
    st->showing = 0;
    st->until = 0;
    out.clear = 1;
    return out;
  }
  if (st->showing) {
    out.show = 1;
    return out;
  }
  return out;
}
