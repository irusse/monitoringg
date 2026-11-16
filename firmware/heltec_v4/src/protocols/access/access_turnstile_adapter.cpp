#include "protocols/access/access_turnstile_adapter.h"

#include <stdio.h>
#include <string.h>

void AccessTurnstileAdapter::setGateState(const char* state) {
  device_copy(gate_state_, sizeof(gate_state_), state && state[0] ? state : "UNKNOWN");
}

void AccessTurnstileAdapter::noteCard(const char* uid, int64_t ts, const char* action,
                                     const char* status, const char* name) {
  if (!uid || !uid[0]) return;
  device_copy(last_uid_, sizeof(last_uid_), uid);
  last_card_ts_ = ts;
  ++last_card_seq_;
  snprintf(last_uuid_, sizeof(last_uuid_), "00000000-0000-4000-8000-%012lu",
           (unsigned long)last_card_seq_);
  device_copy(last_action_, sizeof(last_action_), action && action[0] ? action : "ENTRY");
  device_copy(last_status_, sizeof(last_status_), status && status[0] ? status : "Authorized");
  device_copy(last_name_, sizeof(last_name_), name ? name : "");
}

bool AccessTurnstileAdapter::begin() { return true; }

bool AccessTurnstileAdapter::poll() { return true; }

bool AccessTurnstileAdapter::getInfo(DeviceInfo* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  device_copy(out->id, sizeof(out->id), id());
  device_copy(out->vendor, sizeof(out->vendor), "access");
  device_copy(out->model, sizeof(out->model), "TURNSTILE");
  device_copy(out->protocol, sizeof(out->protocol), protocolKey());
  out->address = 0;
  out->online = true;
  device_copy(out->capabilities[0], DEVICE_CAP_LEN, "READ_STATUS");
  device_copy(out->capabilities[1], DEVICE_CAP_LEN, "ACCESS_OPEN");
  out->cap_count = 2;
  return true;
}

size_t AccessTurnstileAdapter::getPoints(DevicePoint* out, size_t max) {
  if (!out || max == 0) return 0;
  size_t n = 0;
  memset(&out[n], 0, sizeof(out[n]));
  device_copy(out[n].device_id, sizeof(out[n].device_id), id());
  device_copy(out[n].point_id, sizeof(out[n].point_id), "TURNSTILE");
  device_copy(out[n].type, sizeof(out[n].type), "turnstile");
  device_copy(out[n].name, sizeof(out[n].name), "Turnstile");
  device_copy(out[n].state, sizeof(out[n].state), gate_state_);
  ++n;
  if (n >= max) return n;
  memset(&out[n], 0, sizeof(out[n]));
  device_copy(out[n].device_id, sizeof(out[n].device_id), id());
  device_copy(out[n].point_id, sizeof(out[n].point_id), "READER");
  device_copy(out[n].type, sizeof(out[n].type), "card_reader");
  device_copy(out[n].name, sizeof(out[n].name), "Reader");
  device_copy(out[n].state, sizeof(out[n].state), last_uid_[0] ? "SEEN" : "UNKNOWN");
  ++n;
  return n;
}

size_t AccessTurnstileAdapter::getEvents(DeviceEvent* out, size_t max) {
  if (!out || max == 0 || last_card_seq_ == 0) return 0;
  memset(&out[0], 0, sizeof(out[0]));
  device_copy(out[0].id, sizeof(out[0].id), last_uuid_);
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "READER");
  device_copy(out[0].type, sizeof(out[0].type), "CARD");
  device_copy(out[0].severity, sizeof(out[0].severity), "INFO");
  device_copy(out[0].state, sizeof(out[0].state), "SEEN");
  out[0].timestamp = last_card_ts_;
  out[0].sequence = last_card_seq_;
  device_copy(out[0].source, sizeof(out[0].source), "RFID");
  device_copy(out[0].rfid_uid, sizeof(out[0].rfid_uid), last_uid_);
  device_copy(out[0].action, sizeof(out[0].action), last_action_);
  device_copy(out[0].status, sizeof(out[0].status), last_status_);
  device_copy(out[0].name, sizeof(out[0].name), last_name_);
  out[0].device_ok = true;
  out[0].mesh_ok = false;
  return 1;
}
