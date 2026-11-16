#pragma once

#include "device/adapter.h"
#include "protocols/access/access_turnstile_adapter.h"
#include "protocols/bolid/c2000pp_modbus_adapter.h"
#include "protocols/bolid/signal10/signal10_adapter.h"
#include "protocols/bolid/signal20/signal20_adapter.h"
#include "protocols/pcn/pcn_contact.h"
#include "protocols/raw/raw_rs485_adapter.h"

class DeviceRegistry {
public:
  static constexpr size_t kMaxAdapters = 8;
  static constexpr size_t kMaxPoints = 16;
  static constexpr size_t kMaxEvents = 8;

  DeviceRegistry();
  DeviceRegistry(const DeviceRegistry&) = delete;
  DeviceRegistry& operator=(const DeviceRegistry&) = delete;

  RawRs485Adapter& raw() { return raw_; }
  AccessTurnstileAdapter& access() { return access_; }
  C2000ppModbusAdapter& c2000pp() { return c2000pp_; }
  Signal10Adapter& signal10() { return signal10_; }
  Signal20Adapter& signal20() { return signal20_; }
  PcnContactAdapter& pcn() { return pcn_; }
  const RawRs485Adapter& raw() const { return raw_; }
  const AccessTurnstileAdapter& access() const { return access_; }
  const C2000ppModbusAdapter& c2000pp() const { return c2000pp_; }
  const Signal10Adapter& signal10() const { return signal10_; }
  const Signal20Adapter& signal20() const { return signal20_; }
  const PcnContactAdapter& pcn() const { return pcn_; }

  void beginAll();
  void pollAll();
  void bindPort(Mesh485Port* port);
  void setTxBlocked(bool blocked);
  // Manual RS-485 adapter protocol (CONFIG). Not AUTO. Default RAW (PP still probes).
  bool setBusProtocol(const char* proto);
  const char* busProtocol() const { return bus_protocol_; }
  static bool busProtocolAllowed(const char* proto);

  size_t count() const { return n_; }
  size_t onlineCount() const;

  DeviceAdapter* byIndex(size_t i);
  DeviceAdapter* byId(const char* device_id);
  DeviceAdapter* first();

private:
  RawRs485Adapter raw_;
  AccessTurnstileAdapter access_;
  C2000ppModbusAdapter c2000pp_;
  Signal10Adapter signal10_;
  Signal20Adapter signal20_;
  PcnContactAdapter pcn_;
  char bus_protocol_[16] = "RAW";
  DeviceAdapter* slots_[kMaxAdapters] = {};
  size_t n_ = 0;

  bool add(DeviceAdapter* a);
};
