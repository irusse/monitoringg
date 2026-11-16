#include "protocols/raw/raw_rs485_adapter.h"

#include <stdio.h>
#include <string.h>

bool RawRs485Adapter::begin() {
  online_ = port_ && port_->ok();
  return true;
}

bool RawRs485Adapter::poll() {
  online_ = port_ && port_->ok();
  return true;
}

void RawRs485Adapter::noteIdleRx(size_t len) {
  if (len == 0) return;
  last_rx_len_ = len;
  ++last_rx_seq_;
}

bool RawRs485Adapter::getInfo(DeviceInfo* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  device_copy(out->id, sizeof(out->id), id());
  device_copy(out->vendor, sizeof(out->vendor), "generic");
  device_copy(out->model, sizeof(out->model), "RS485");
  device_copy(out->protocol, sizeof(out->protocol), protocolKey());
  out->address = 0;
  out->online = port_ && port_->ok();
  device_copy(out->capabilities[0], DEVICE_CAP_LEN, "READ_STATUS");
  out->cap_count = 1;
  return true;
}

size_t RawRs485Adapter::getPoints(DevicePoint* out, size_t max) {
  if (!out || max == 0) return 0;
  memset(&out[0], 0, sizeof(out[0]));
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "BUS");
  device_copy(out[0].type, sizeof(out[0].type), "loop");
  device_copy(out[0].name, sizeof(out[0].name), "RS-485");
  device_copy(out[0].state, sizeof(out[0].state), "UNKNOWN");
  return 1;
}

size_t RawRs485Adapter::getEvents(DeviceEvent* out, size_t max) {
  if (!out || max == 0 || last_rx_seq_ == 0) return 0;
  memset(&out[0], 0, sizeof(out[0]));
  char uuid[DEVICE_EVENT_ID_MAX];
  snprintf(uuid, sizeof(uuid), "00000000-0000-4000-8001-%012lu", (unsigned long)last_rx_seq_);
  device_copy(out[0].id, sizeof(out[0].id), uuid);
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "BUS");
  device_copy(out[0].type, sizeof(out[0].type), "DIAG");
  device_copy(out[0].severity, sizeof(out[0].severity), "INFO");
  device_copy(out[0].state, sizeof(out[0].state), "SEEN");
  out[0].timestamp = 0;
  out[0].sequence = last_rx_seq_;
  device_copy(out[0].source, sizeof(out[0].source), "RS485");
  out[0].device_ok = false;
  out[0].mesh_ok = false;
  (void)last_rx_len_;
  return 1;
}
