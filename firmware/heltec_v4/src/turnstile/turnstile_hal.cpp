#include "turnstile/turnstile_hal.h"
#include "access/pins.h"
#include <Arduino.h>

void TurnstileHAL::relayOff() {
#ifndef ACCESS_STUB_NO_RADIO
  digitalWrite(PIN_RELAY_OPEN, LOW);
#endif
}

void TurnstileHAL::applyLamps() {
  switch (state_) {
    case TurnstileState::Opening:
    case TurnstileState::Open:
      lampGreen(true);
      lampRed(false);
      break;
    case TurnstileState::Emergency:
    case TurnstileState::Error:
      lampGreen(false);
      lampRed(true);
      break;
    case TurnstileState::Closing:
    case TurnstileState::Locked:
    default:
      lampGreen(false);
      lampRed(true);
      break;
  }
}

void TurnstileHAL::enterClosing() {
  relayOff();
  state_ = TurnstileState::Closing;
  close_until_ = millis() + kCloseMs;
  applyLamps();
}

void TurnstileHAL::begin() {
#ifndef ACCESS_STUB_NO_RADIO
  pinMode(PIN_RELAY_OPEN, OUTPUT);
  digitalWrite(PIN_RELAY_OPEN, LOW);
  pinMode(PIN_LAMP_GREEN, OUTPUT);
#ifndef HELTEC_LORA_V4_TFT
  pinMode(PIN_LAMP_RED, OUTPUT);
#endif
  pinMode(PIN_SENSOR_ENTRY, INPUT_PULLUP);
  pinMode(PIN_SENSOR_EXIT, INPUT_PULLUP);
  pinMode(PIN_EMERGENCY, INPUT_PULLUP);
#endif
  state_ = TurnstileState::Locked;
  pulse_until_ = 0;
  hold_until_ = 0;
  close_until_ = 0;
  applyLamps();
}

bool TurnstileHAL::entryAsserted() const {
#ifndef ACCESS_STUB_NO_RADIO
  return digitalRead(PIN_SENSOR_ENTRY) == LOW;
#else
  return false;
#endif
}

bool TurnstileHAL::exitAsserted() const {
#ifndef ACCESS_STUB_NO_RADIO
  return digitalRead(PIN_SENSOR_EXIT) == LOW;
#else
  return false;
#endif
}

bool TurnstileHAL::emergencyAsserted() const {
#ifndef ACCESS_STUB_NO_RADIO
  // NC loop is optional; unconnected pin stays HIGH via pull-up = idle.
  // Press / open-collector to GND asserts emergency (active LOW).
  return digitalRead(PIN_EMERGENCY) == LOW;
#else
  return false;
#endif
}

void TurnstileHAL::loop() {
  const uint32_t now = millis();

  if (emergencyAsserted()) {
    if (state_ != TurnstileState::Emergency) emergency();
    return;
  }

  if (state_ == TurnstileState::Emergency) {
    lock();
    return;
  }

  switch (state_) {
    case TurnstileState::Opening:
      if (now >= pulse_until_) {
        relayOff();
        state_ = TurnstileState::Open;
        hold_until_ = now + kHoldMs;
        applyLamps();
      }
      break;
    case TurnstileState::Open:
      if (entryAsserted() || exitAsserted() || now >= hold_until_) {
        enterClosing();
      }
      break;
    case TurnstileState::Closing:
      if (now >= close_until_) {
        lock();
      }
      break;
    default:
      break;
  }
}

void TurnstileHAL::openPulse(uint16_t ms) {
  if (emergencyAsserted()) {
    emergency();
    return;
  }
  state_ = TurnstileState::Opening;
  pulse_until_ = millis() + ms;
#ifndef ACCESS_STUB_NO_RADIO
  digitalWrite(PIN_RELAY_OPEN, HIGH);
#endif
  applyLamps();
}

void TurnstileHAL::lock() {
  relayOff();
  state_ = TurnstileState::Locked;
  applyLamps();
}

void TurnstileHAL::unlock() {
  if (emergencyAsserted()) {
    emergency();
    return;
  }
  openPulse(300);
}

void TurnstileHAL::emergency() {
  relayOff();
  state_ = TurnstileState::Emergency;
  applyLamps();
}

TurnstileState TurnstileHAL::state() const { return state_; }

void TurnstileHAL::lampGreen(bool on) {
#ifndef ACCESS_STUB_NO_RADIO
  digitalWrite(PIN_LAMP_GREEN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

void TurnstileHAL::lampRed(bool on) {
#if !defined(ACCESS_STUB_NO_RADIO) && !defined(HELTEC_LORA_V4_TFT)
  digitalWrite(PIN_LAMP_RED, on ? HIGH : LOW);
#else
  (void)on;
#endif
}
