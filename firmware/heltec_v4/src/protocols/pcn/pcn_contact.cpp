#include "protocols/pcn/pcn_contact.h"

#include "mesh485/clock.h"

#include <stdio.h>
#include <string.h>

#ifndef MESH485_NATIVE
#include "access/pins.h"
static_assert(PIN_PCN_ALARM == PcnContactAdapter::kPin, "PCN contact is GPIO 47");
#endif

bool PcnContactAdapter::begin() {
  armed_ = false;
  seen_ = false;
  raw_closed_ = false;
  raw_since_ = 0;
  have_level_ = false;
  level_closed_ = false;
  have_event_ = false;
  episode_ = false;
  episode_uuid_[0] = 0;
  edge_ts_ = 0;
  uuid_[0] = 0;
  return true;
}

#ifdef MESH485_DEBUG
void PcnContactAdapter::debugSetClosed(bool closed) {
  debug_override_ = true;
  debug_closed_ = closed;
}

bool PcnContactAdapter::gpioClosed() const {
#ifndef MESH485_NATIVE
  return digitalRead(PIN_PCN_ALARM) == LOW;
#else
  return false;
#endif
}
#endif

void PcnContactAdapter::armPin() {
#ifndef MESH485_NATIVE
  if (armed_) return;
  pinMode(PIN_PCN_ALARM, INPUT_PULLUP);
  pinMode(PIN_PCN_ALARM_B, INPUT_PULLUP);
  armed_ = true;
#else
  (void)armed_;
#endif
}

bool PcnContactAdapter::readClosed() const {
#ifdef MESH485_DEBUG
  if (debug_override_) return debug_closed_;
#endif
#ifdef MESH485_NATIVE
  return injected_closed_ || injected_b_;
#else
  return digitalRead(PIN_PCN_ALARM) == LOW || digitalRead(PIN_PCN_ALARM_B) == LOW;
#endif
}

void PcnContactAdapter::publishEdge(uint32_t now_ms) {
  ++edge_seq_;
  snprintf(uuid_, sizeof(uuid_), "00000000-0000-4000-8005-%012lu",
           (unsigned long)(edge_seq_ + now_ms));
  edge_ts_ = (int64_t)(now_ms / 1000u);
  have_event_ = true;
  if (level_closed_) {
    episode_ = true;
    snprintf(episode_uuid_, sizeof(episode_uuid_), "%s", uuid_);
  } else {
    episode_ = false;
  }
}

bool PcnContactAdapter::poll() {
  if (!listen_) return true;
  armPin();
  const uint32_t now = mesh485_now_ms();
  const bool closed = readClosed();
  if (!seen_) {
    seen_ = true;
    raw_closed_ = closed;
    raw_since_ = now;
    return true;
  }
  if (closed != raw_closed_) {
    raw_closed_ = closed;
    raw_since_ = now;
    return true;
  }
  if ((uint32_t)(now - raw_since_) < kDebounceMs) return true;
  if (!have_level_) {
    have_level_ = true;
    level_closed_ = raw_closed_;
#ifdef MESH485_DEBUG
    Serial.printf("PCN first level closed=%d (not an event)\n", level_closed_ ? 1 : 0);
#endif
    return true;
  }
  if (level_closed_ == raw_closed_) return true;
  level_closed_ = raw_closed_;
  publishEdge(now);
#ifdef MESH485_DEBUG
  Serial.printf("PCN edge %s\n", level_closed_ ? "ALARM" : "NORMAL");
#endif
  return true;
}

bool PcnContactAdapter::getInfo(DeviceInfo* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  device_copy(out->id, sizeof(out->id), id());
  device_copy(out->vendor, sizeof(out->vendor), "local");
  device_copy(out->model, sizeof(out->model), "PCN");
  device_copy(out->protocol, sizeof(out->protocol), protocolKey());
  out->address = 0;
  out->online = true;
  device_copy(out->capabilities[0], DEVICE_CAP_LEN, "READ_STATUS");
  out->cap_count = 1;
  return true;
}

size_t PcnContactAdapter::getPoints(DevicePoint* out, size_t max) {
  if (!out || max == 0) return 0;
  memset(&out[0], 0, sizeof(out[0]));
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "ALARM");
  device_copy(out[0].type, sizeof(out[0].type), "PCN");
  device_copy(out[0].name, sizeof(out[0].name), "PCN");
  device_copy(out[0].state, sizeof(out[0].state), overlayState());
  return 1;
}

size_t PcnContactAdapter::getEvents(DeviceEvent* out, size_t max) {
  if (!out || max == 0 || !have_event_ || !uuid_[0]) return 0;
  memset(&out[0], 0, sizeof(out[0]));
  device_copy(out[0].id, sizeof(out[0].id), uuid_);
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "ALARM");
  device_copy(out[0].type, sizeof(out[0].type), "PCN");
  device_copy(out[0].severity, sizeof(out[0].severity),
              level_closed_ ? "ALARM" : "INFO");
  device_copy(out[0].state, sizeof(out[0].state), level_closed_ ? "ALARM" : "NORMAL");
  out[0].timestamp = edge_ts_;
  out[0].sequence = edge_seq_;
  device_copy(out[0].source, sizeof(out[0].source), "GPIO");
  out[0].device_ok = true;
  out[0].mesh_ok = false;
  return 1;
}
