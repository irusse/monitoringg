#pragma once

#include <stdint.h>

enum class TurnstileState {
  Locked,
  Opening,
  Open,
  Closing,
  Error,
  Emergency
};

class TurnstileHAL {
public:
  void begin();
  void loop();
  void openPulse(uint16_t ms = 300);
  void lock();
  void unlock();
  void emergency();
  TurnstileState state() const;
  bool entryAsserted() const;
  bool exitAsserted() const;
  bool emergencyAsserted() const;
  void lampGreen(bool on);
  void lampRed(bool on);

private:
  static constexpr uint32_t kHoldMs = 5000;
  static constexpr uint32_t kCloseMs = 400;

  TurnstileState state_ = TurnstileState::Locked;
  uint32_t pulse_until_ = 0;
  uint32_t hold_until_ = 0;
  uint32_t close_until_ = 0;

  void applyLamps();
  void relayOff();
  void enterClosing();
};
