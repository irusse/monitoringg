#pragma once

#include "device/adapter.h"

// Panel-node dry contact for the С2000-М PCN «Тревога» relay.
// GPIO 47, INPUT_PULLUP, active LOW through an optocoupler to GND.
// A stable edge to LOW is one PCN/ALARM. A stable edge to HIGH is one
// PCN/NORMAL. Holding a level does not enqueue another row.
// The first stable sample (open input at boot) is not an event.
// This is not an Orion frame and it does not drive GPIO 40.
class PcnContactAdapter : public DeviceAdapter {
public:
  static constexpr int kPin = 47;
  static constexpr uint32_t kDebounceMs = 50;

  const char* id() const override { return "PCN-001"; }
  const char* protocolKey() const override { return "PCN_CONTACT"; }

  bool begin() override;
  bool poll() override;
  bool getInfo(DeviceInfo* out) override;
  size_t getPoints(DevicePoint* out, size_t max) override;
  size_t getEvents(DeviceEvent* out, size_t max) override;

  // Debounced contact. False before the first stable sample and while open.
  // GPIO 47 or GPIO 45, either one.
  bool alarm() const { return have_level_ && level_closed_; }
  // True only after an ALARM edge, until the matching NORMAL edge.
  // The first sample at boot is not an episode, so a held contact does not transmit.
  bool episodeAlarm() const { return episode_; }
  const char* episodeUuid() const { return episode_uuid_; }
  const char* overlayState() const { return alarm() ? "ALARM" : "NORMAL"; }
  // Receiver role does not sample the pins. Default is listen (source / tests).
  void setListen(bool on) { listen_ = on; }

#ifdef MESH485_NATIVE
  // Native tests inject the pin. LOW/closed = true. Default is open.
  // The two inputs are OR-ed.
  void setRawClosed(bool closed) { injected_closed_ = closed; }
  void setRawClosed2(bool closed) { injected_b_ = closed; }
#endif
#ifdef MESH485_DEBUG
  // Bench inject. The real GPIO is still reported by gpioClosed().
  void debugSetClosed(bool closed);
  bool gpioClosed() const;
#endif

private:
  bool armed_ = false;
  bool listen_ = true;
#ifdef MESH485_NATIVE
  bool injected_closed_ = false;
  bool injected_b_ = false;
#endif
  bool episode_ = false;
  char episode_uuid_[DEVICE_EVENT_ID_MAX] = {};
  bool seen_ = false;
  bool raw_closed_ = false;
  uint32_t raw_since_ = 0;
  bool have_level_ = false;
  bool level_closed_ = false;
  bool have_event_ = false;
  uint32_t edge_seq_ = 0;
  int64_t edge_ts_ = 0;
  char uuid_[DEVICE_EVENT_ID_MAX] = {};
#ifdef MESH485_DEBUG
  bool debug_override_ = false;
  bool debug_closed_ = false;
#endif

  void armPin();
  bool readClosed() const;
  void publishEdge(uint32_t now_ms);
};
