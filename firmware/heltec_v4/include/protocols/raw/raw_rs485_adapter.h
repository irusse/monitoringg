#pragma once

#include "device/adapter.h"
#include "mesh485/port.h"

// Listen/status only. Never writeFrame. Does not parse Modbus/Bolid.
class RawRs485Adapter : public DeviceAdapter {
public:
  const char* id() const override { return "RAW-001"; }
  const char* protocolKey() const override { return "RAW_RS485"; }

  void bindPort(Mesh485Port* port) { port_ = port; }
  // Optional diagnostic: last idle RX length. Empty events if never called.
  void noteIdleRx(size_t len);

  bool begin() override;
  bool poll() override;
  bool getInfo(DeviceInfo* out) override;
  size_t getPoints(DevicePoint* out, size_t max) override;
  size_t getEvents(DeviceEvent* out, size_t max) override;

private:
  Mesh485Port* port_ = nullptr;
  bool online_ = false;
  size_t last_rx_len_ = 0;
  uint32_t last_rx_seq_ = 0;
};
