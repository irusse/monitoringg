#include "device/registry.h"

#include <string.h>

DeviceRegistry::DeviceRegistry() {
  add(&raw_);
  add(&access_);
  add(&c2000pp_);
  add(&signal10_);
  add(&signal20_);
  add(&pcn_);
}

bool DeviceRegistry::busProtocolAllowed(const char* proto) {
  if (!proto || !proto[0]) return false;
  return strcmp(proto, "RAW") == 0 || strcmp(proto, "BOLID_C2000PP") == 0 ||
         strcmp(proto, "BOLID_SIGNAL10") == 0 || strcmp(proto, "BOLID_SIGNAL20") == 0;
}

bool DeviceRegistry::setBusProtocol(const char* proto) {
  if (!busProtocolAllowed(proto)) return false;
  device_copy(bus_protocol_, sizeof(bus_protocol_), proto);
  const bool s10 = strcmp(bus_protocol_, "BOLID_SIGNAL10") == 0;
  const bool s20 = strcmp(bus_protocol_, "BOLID_SIGNAL20") == 0;
  c2000pp_.setSelected(!s10 && !s20);
  signal10_.setSelected(s10);
  signal20_.setSelected(s20);
  return true;
}

bool DeviceRegistry::add(DeviceAdapter* a) {
  if (!a || n_ >= kMaxAdapters) return false;
  slots_[n_++] = a;
  return true;
}

void DeviceRegistry::beginAll() {
  for (size_t i = 0; i < n_; ++i) {
    if (slots_[i]) slots_[i]->begin();
  }
}

void DeviceRegistry::pollAll() {
  for (size_t i = 0; i < n_; ++i) {
    if (slots_[i]) slots_[i]->poll();
  }
}

void DeviceRegistry::bindPort(Mesh485Port* port) {
  raw_.bindPort(port);
  c2000pp_.bindPort(port);
  signal10_.bindPort(port);
  signal20_.bindPort(port);
}

void DeviceRegistry::setTxBlocked(bool blocked) {
  c2000pp_.setTxBlocked(blocked);
  signal10_.setTxBlocked(blocked);
  signal20_.setTxBlocked(blocked);
}

size_t DeviceRegistry::onlineCount() const {
  size_t n = 0;
  DeviceInfo info;
  for (size_t i = 0; i < n_; ++i) {
    if (!slots_[i]) continue;
    if (slots_[i]->getInfo(&info) && info.online) ++n;
  }
  return n;
}

DeviceAdapter* DeviceRegistry::byIndex(size_t i) {
  return i < n_ ? slots_[i] : nullptr;
}

DeviceAdapter* DeviceRegistry::byId(const char* device_id) {
  if (!device_id || !device_id[0]) return nullptr;
  for (size_t i = 0; i < n_; ++i) {
    if (slots_[i] && strcmp(slots_[i]->id(), device_id) == 0) return slots_[i];
  }
  return nullptr;
}

DeviceAdapter* DeviceRegistry::first() { return n_ > 0 ? slots_[0] : nullptr; }
