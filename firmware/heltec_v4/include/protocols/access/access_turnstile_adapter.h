#pragma once

#include "device/adapter.h"

// GPIO Access path as a Device/Point/Event. OPEN is ACCESS_OPEN, not COMMAND_EXECUTE.
class AccessTurnstileAdapter : public DeviceAdapter {
public:
  const char* id() const override { return "TS-001"; }
  const char* protocolKey() const override { return "ACCESS"; }

  // LOCKED / OPEN / EMERGENCY / UNKNOWN — filled from TurnstileHAL / AccessGate.
  void setGateState(const char* state);
  void noteCard(const char* uid, int64_t ts, const char* action = nullptr,
                const char* status = nullptr, const char* name = nullptr);
  const char* lastUid() const { return last_uid_; }

  bool begin() override;
  bool poll() override;
  bool getInfo(DeviceInfo* out) override;
  size_t getPoints(DevicePoint* out, size_t max) override;
  size_t getEvents(DeviceEvent* out, size_t max) override;

private:
  char gate_state_[16] = "LOCKED";
  char last_uid_[24] = {};
  char last_uuid_[DEVICE_EVENT_ID_MAX] = {};
  char last_action_[16] = {};
  char last_status_[16] = {};
  char last_name_[32] = {};
  int64_t last_card_ts_ = 0;
  uint32_t last_card_seq_ = 0;
};
