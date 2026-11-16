#pragma once

#include <stdint.h>
#include <string.h>

// RAM turnstile used by MESH 485 ACCESS_OPEN / ACCESS_LOCK.
// Device builds also pulse GPIO via AccessHwHooks. Native tests have no GPIO;
// device_ok follows this fake HAL (lastOpenOk / lastLockOk), not a synthesized click.
class AccessGate {
public:
  void openPulse(uint16_t ms = 300);
  void lock();
  const char* stateName() const { return state_; }
  bool lastOpenOk() const { return last_open_ok_; }
  bool lastLockOk() const { return last_lock_ok_; }
  bool lastRemote() const { return last_remote_; }
  void noteRemote() { last_remote_ = true; }

private:
  char state_[16] = "LOCKED";
  bool last_open_ok_ = false;
  bool last_lock_ok_ = false;
  bool last_remote_ = false;
};

inline void AccessGate::openPulse(uint16_t ms) {
  (void)ms;
  if (strcmp(state_, "EMERGENCY") == 0) {
    last_open_ok_ = false;
    return;
  }
  strncpy(state_, "OPEN", sizeof(state_) - 1);
  state_[sizeof(state_) - 1] = 0;
  last_open_ok_ = true;
}

inline void AccessGate::lock() {
  if (strcmp(state_, "EMERGENCY") == 0) {
    last_lock_ok_ = false;
    return;
  }
  strncpy(state_, "LOCKED", sizeof(state_) - 1);
  state_[sizeof(state_) - 1] = 0;
  last_lock_ok_ = true;
}

struct AccessHwHooks {
  void (*openPulse)(uint16_t ms, void* ctx) = nullptr;
  void (*lock)(void* ctx) = nullptr;
  void (*showRemote)(const char* cmd, const char* optr, void* ctx) = nullptr;
  void* ctx = nullptr;
};
