#include "protocols/bolid/signal20/signal20_adapter.h"

#include "mesh485/clock.h"

#include <stdio.h>
#include <string.h>

void Signal20Adapter::setSelected(bool on) {
  selected_ = on;
  in_flight_ = false;
  if (on) next_tx_ms_ = mesh485_now_ms() + kPollIntervalMs;
}

const char* Signal20Adapter::overlayState() const {
  if (online_) return "NORMAL";
  return "FAULT";
}

bool Signal20Adapter::begin() {
  online_ = false;
  have_zone1_ = false;
  in_flight_ = false;
  diag_event_ = false;
  next_tx_ms_ = kPollIntervalMs;
  return true;
}

bool Signal20Adapter::acceptStatusReply(const uint8_t* frame, size_t len) {
  if (!frameCrcOk(frame, len)) return false;
  if (framePeer(frame) != addr_) return false;
  if (!online_) {
    ++diag_seq_;
    snprintf(diag_uuid_, sizeof(diag_uuid_), "00000000-0000-4000-8004-%012lu",
             (unsigned long)diag_seq_);
    diag_event_ = true;
  }
  online_ = true;
  have_zone1_ = true;  // product has ~20 ШС; dump has no per-zone bitmap → UNKNOWN
  return true;
}

void Signal20Adapter::startTxn() {
  if (!port_ || tx_blocked_ || !selected_) return;
  uint8_t req[kStatusReqLen];
  const size_t n = buildStatusRequest(req, sizeof(req), addr_);
  if (!n) return;
  if (port_->writeFrame(req, n) != n) {
    online_ = false;
    have_zone1_ = false;
    in_flight_ = false;
    return;
  }
  in_flight_ = true;
  tx_at_ms_ = mesh485_now_ms();
}

bool Signal20Adapter::poll() {
  if (!selected_ || tx_blocked_) {
    in_flight_ = false;
    return true;
  }
  if (!port_) return true;

  const uint32_t now = mesh485_now_ms();

  if (in_flight_) {
    uint8_t buf[MESH485_FRAME_MAX];
    const size_t n = port_->readFrame(buf, sizeof(buf), 1);
    if (n && acceptStatusReply(buf, n)) {
      in_flight_ = false;
      next_tx_ms_ = now + kPollIntervalMs;
      return true;
    }
    if ((now - tx_at_ms_) >= kTxnTimeoutMs) {
      online_ = false;
      have_zone1_ = false;
      in_flight_ = false;
      next_tx_ms_ = now + kPollIntervalMs;
    }
    return true;
  }

  if (now < next_tx_ms_) return true;
  next_tx_ms_ = now + kPollIntervalMs;
  startTxn();
  if (in_flight_) {
    uint8_t buf[MESH485_FRAME_MAX];
    const size_t n = port_->readFrame(buf, sizeof(buf), 1);
    if (n && acceptStatusReply(buf, n)) {
      in_flight_ = false;
    }
  }
  return true;
}

bool Signal20Adapter::getInfo(DeviceInfo* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  device_copy(out->id, sizeof(out->id), id());
  device_copy(out->vendor, sizeof(out->vendor), "bolid");
  device_copy(out->model, sizeof(out->model), "SIGNAL20");
  device_copy(out->protocol, sizeof(out->protocol), protocolKey());
  out->address = addr_;
  out->online = online_;
  device_copy(out->capabilities[0], DEVICE_CAP_LEN, "READ_STATUS");
  out->cap_count = 1;
  if (have_zone1_ && out->cap_count < DEVICE_CAP_MAX) {
    device_copy(out->capabilities[out->cap_count], DEVICE_CAP_LEN, "READ_ZONES");
    ++out->cap_count;
  }
  return true;
}

size_t Signal20Adapter::getPoints(DevicePoint* out, size_t max) {
  if (!out || max == 0) return 0;
  size_t n = 0;
  memset(&out[n], 0, sizeof(out[n]));
  device_copy(out[n].device_id, sizeof(out[n].device_id), id());
  device_copy(out[n].point_id, sizeof(out[n].point_id), "S20");
  device_copy(out[n].type, sizeof(out[n].type), "loop");
  device_copy(out[n].name, sizeof(out[n].name), "Signal-20");
  device_copy(out[n].state, sizeof(out[n].state), overlayState());
  ++n;
  if (have_zone1_ && n < max) {
    memset(&out[n], 0, sizeof(out[n]));
    device_copy(out[n].device_id, sizeof(out[n].device_id), id());
    device_copy(out[n].point_id, sizeof(out[n].point_id), "Z1");
    device_copy(out[n].type, sizeof(out[n].type), "zone");
    device_copy(out[n].name, sizeof(out[n].name), "Zone 1");
    device_copy(out[n].state, sizeof(out[n].state), "UNKNOWN");
    ++n;
  }
  return n;
}

size_t Signal20Adapter::getEvents(DeviceEvent* out, size_t max) {
  if (!out || max == 0 || !diag_event_ || !online_) return 0;
  memset(&out[0], 0, sizeof(*out));
  device_copy(out[0].id, sizeof(out[0].id), diag_uuid_);
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "S20");
  device_copy(out[0].type, sizeof(out[0].type), "DIAG");
  device_copy(out[0].severity, sizeof(out[0].severity), "INFO");
  device_copy(out[0].state, sizeof(out[0].state), "ONLINE");
  out[0].timestamp = 0;
  out[0].sequence = diag_seq_;
  device_copy(out[0].source, sizeof(out[0].source), "RS485");
  out[0].device_ok = true;
  out[0].mesh_ok = false;
  return 1;
}
