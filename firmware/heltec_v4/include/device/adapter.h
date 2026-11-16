#pragma once

#include "device/device_types.h"

class DeviceAdapter {
public:
  virtual ~DeviceAdapter() {}
  virtual const char* id() const = 0;
  virtual const char* protocolKey() const = 0;
  virtual bool begin() = 0;
  virtual bool poll() = 0;  // non-blocking; called from loop()
  virtual bool getInfo(DeviceInfo* out) = 0;
  virtual size_t getPoints(DevicePoint* out, size_t max) = 0;
  virtual size_t getEvents(DeviceEvent* out, size_t max) = 0;
  // Default: no CONTROL. Adapters that advertise CONTROL_RELAY override this.
  virtual bool execute(const CommandRequest& req, CommandOutcome* out) {
    (void)req;
    if (out) {
      out->mesh_ok = true;
      out->device_ok = false;
      device_copy(out->error, sizeof(out->error), "NO_PERMISSION");
    }
    return false;
  }
};
