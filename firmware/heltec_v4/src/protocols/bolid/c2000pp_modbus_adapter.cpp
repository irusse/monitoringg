#include "protocols/bolid/c2000pp_modbus_adapter.h"

#include "mesh485/clock.h"
#include "protocols/modbus/crc16.h"

#include <stdio.h>
#include <string.h>

// hass-hubc2000pp families → Point.state. Codes only; no HUB IP strings.
static bool codeIn(uint16_t code, const uint8_t* set, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (set[i] == (uint8_t)code) return true;
  }
  return false;
}

const char* C2000ppModbusAdapter::zoneStateName(uint16_t code) const {
  static const uint8_t kFire[] = {19, 37, 40, 44, 76};
  static const uint8_t kAlarm[] = {3,  15, 18, 27, 33,  37,  40,  44,  45,  58,
                                  79, 137, 138, 139, 141, 144, 145, 146, 147, 149,
                                  161, 214, 216, 229, 230, 252};
  static const uint8_t kArmed[] = {1,  24, 35, 39, 47,  72,  78,  80,  83, 91,
                                  123, 188, 191, 195, 199, 218, 241};
  static const uint8_t kDisarmed[] = {109, 112, 117, 119, 242};
  if (code == 0) return "NORMAL";
  if (code == 250) return "FAULT";
  if (codeIn(code, kFire, sizeof(kFire)) || codeIn(code, kAlarm, sizeof(kAlarm))) {
    return "ALARM";
  }
  if (codeIn(code, kArmed, sizeof(kArmed))) return "ARMED";
  if (codeIn(code, kDisarmed, sizeof(kDisarmed))) return "DISARMED";
  return "UNKNOWN";
}

const char* C2000ppModbusAdapter::overlayState() const {
  if (alarm_) return "ALARM";
  if (online_) return "NORMAL";
  return "FAULT";
}

void C2000ppModbusAdapter::allowRelay(uint8_t n, bool on) {
  if (n < 1 || n > kMaxNamedRelays) return;
  relay_allow_[n - 1] = on;
  if (!on) relay_state_[n - 1][0] = 0;
}

bool C2000ppModbusAdapter::relayAllowed(uint8_t n) const {
  if (n < 1 || n > kMaxNamedRelays) return false;
  return relay_allow_[n - 1];
}

bool C2000ppModbusAdapter::anyRelayAllowed() const {
  for (uint8_t i = 0; i < kMaxNamedRelays; ++i) {
    if (relay_allow_[i]) return true;
  }
  return false;
}

bool C2000ppModbusAdapter::parseRelayPoint(const char* point_id, uint8_t* n_out) {
  if (!point_id || point_id[0] != 'R' || !point_id[1] || !n_out) return false;
  int n = 0;
  for (const char* p = point_id + 1; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
  }
  if (n < 1 || n > (int)kMaxNamedRelays) return false;
  *n_out = (uint8_t)n;
  return true;
}

bool C2000ppModbusAdapter::parseOnOff(const char* action, bool* on_out) {
  if (!action || !on_out) return false;
  if (strcmp(action, "ON") == 0 || strcmp(action, "on") == 0) {
    *on_out = true;
    return true;
  }
  if (strcmp(action, "OFF") == 0 || strcmp(action, "off") == 0) {
    *on_out = false;
    return true;
  }
  return false;
}

void C2000ppModbusAdapter::setSelected(bool on) {
  selected_ = on;
  if (!on) in_flight_ = false;
}

bool C2000ppModbusAdapter::begin() {
  online_ = false;
  alarm_ = false;
  in_flight_ = false;
  have_zone1_ = false;
  type_reg_ = 0;
  version_reg_ = 0;
  zone1_code_ = 0;
  pending_kind_ = 0;
  diag_event_ = false;
  next_tx_ms_ = kPollIntervalMs;
#if MESH485_ALLOW_PP_RELAY
  if (!anyRelayAllowed()) allowRelay(1, true);
#endif
  return true;
}

size_t C2000ppModbusAdapter::buildReadHolding(uint8_t* adu, size_t max, uint16_t start,
                                              uint16_t qty) const {
  if (!adu || max < 8) return 0;
  adu[0] = slave_;
  adu[1] = 0x03;
  adu[2] = (uint8_t)(start >> 8);
  adu[3] = (uint8_t)(start & 0xFF);
  adu[4] = (uint8_t)(qty >> 8);
  adu[5] = (uint8_t)(qty & 0xFF);
  return crc16_modbus_append(adu, 6, max);
}

size_t C2000ppModbusAdapter::buildWriteCoil(uint8_t* adu, size_t max, uint16_t coil,
                                            bool on) const {
  if (!adu || max < 8) return 0;
  adu[0] = slave_;
  adu[1] = 0x05;
  adu[2] = (uint8_t)(coil >> 8);
  adu[3] = (uint8_t)(coil & 0xFF);
  adu[4] = on ? 0xFF : 0x00;
  adu[5] = 0x00;
  return crc16_modbus_append(adu, 6, max);
}

bool C2000ppModbusAdapter::acceptFc05(const uint8_t* adu, size_t len) const {
  if (!adu || !crc16_modbus_ok(adu, len)) return false;
  if (adu[0] != slave_) return false;
  if (adu[1] & 0x80) return false;
  return adu[1] == 0x05 && len >= 8;
}

bool C2000ppModbusAdapter::acceptFc03(const uint8_t* adu, size_t len) {
  if (!adu || !crc16_modbus_ok(adu, len)) return false;
  if (adu[0] != slave_) return false;
  if (adu[1] & 0x80) return false;  // exception — not a successful read
  if (adu[1] != 0x03 || len < 5) return false;
  const uint8_t bc = adu[2];
  if ((size_t)(3 + bc + 2) != len || (bc & 1u) || bc == 0) return false;

  if (pending_kind_ == 0) {
    if (bc < 4) return false;
    type_reg_ = (uint16_t)(((uint16_t)adu[3] << 8) | adu[4]);
    version_reg_ = (uint16_t)(((uint16_t)adu[5] << 8) | adu[6]);
    if (!online_) {
      ++diag_seq_;
      snprintf(diag_uuid_, sizeof(diag_uuid_), "00000000-0000-4000-8002-%012lu",
               (unsigned long)diag_seq_);
      diag_event_ = true;
    }
    online_ = true;
    alarm_ = false;
    // Optional next slice only for a real С2000-ПП (type 36). Do not invent 36.
    pending_kind_ = (type_reg_ == 36) ? 1 : 0;
    return true;
  }

  if (pending_kind_ == 1) {
    if (bc < 2) return false;
    zone1_code_ = (uint16_t)(((uint16_t)adu[3] << 8) | adu[4]);
    have_zone1_ = true;
    const char* st = zoneStateName(zone1_code_);
    alarm_ = (strcmp(st, "ALARM") == 0);
    pending_kind_ = 0;
    return true;
  }
  return false;
}

void C2000ppModbusAdapter::startTxn(uint8_t kind) {
  if (!port_ || tx_blocked_) return;
  uint16_t start = kProbeStart;
  uint16_t qty = kProbeQty;
  if (kind == 1) {
    start = 40000;  // zone №1 state, table 1.1.5.14.1
    qty = 1;
  }
  uint8_t adu[8];
  const size_t n = buildReadHolding(adu, sizeof(adu), start, qty);
  if (!n) return;
  if (port_->writeFrame(adu, n) != n) {
    online_ = false;
    in_flight_ = false;
    return;
  }
  pending_kind_ = kind;
  in_flight_ = true;
  tx_at_ms_ = mesh485_now_ms();
}

bool C2000ppModbusAdapter::poll() {
  if (!selected_ || tx_blocked_) {
    in_flight_ = false;
    return true;
  }
  if (!port_) return true;

  const uint32_t now = mesh485_now_ms();

  if (in_flight_) {
    uint8_t buf[MESH485_FRAME_MAX];
    const size_t n = port_->readFrame(buf, sizeof(buf), 1);
    if (n && acceptFc03(buf, n)) {
      in_flight_ = false;
      next_tx_ms_ = now + kPollIntervalMs;
      return true;
    }
    if ((now - tx_at_ms_) >= kTxnTimeoutMs) {
      online_ = false;
      alarm_ = false;
      have_zone1_ = false;
      type_reg_ = 0;
      version_reg_ = 0;
      pending_kind_ = 0;
      in_flight_ = false;
      next_tx_ms_ = now + kPollIntervalMs;
    }
    return true;
  }

  if (now < next_tx_ms_) return true;
  next_tx_ms_ = now + kPollIntervalMs;
  startTxn(pending_kind_);
  if (in_flight_) {
    uint8_t buf[MESH485_FRAME_MAX];
    const size_t n = port_->readFrame(buf, sizeof(buf), 1);
    if (n && acceptFc03(buf, n)) {
      in_flight_ = false;
    }
  }
  return true;
}

bool C2000ppModbusAdapter::getInfo(DeviceInfo* out) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  device_copy(out->id, sizeof(out->id), id());
  device_copy(out->vendor, sizeof(out->vendor), "bolid");
  device_copy(out->model, sizeof(out->model), "C2000PP");
  device_copy(out->protocol, sizeof(out->protocol), protocolKey());
  out->address = slave_;
  out->online = online_;
  device_copy(out->capabilities[0], DEVICE_CAP_LEN, "READ_STATUS");
  out->cap_count = 1;
  if (have_zone1_ && out->cap_count < DEVICE_CAP_MAX) {
    device_copy(out->capabilities[out->cap_count], DEVICE_CAP_LEN, "READ_ZONES");
    ++out->cap_count;
  }
  if (anyRelayAllowed() && out->cap_count < DEVICE_CAP_MAX) {
    device_copy(out->capabilities[out->cap_count], DEVICE_CAP_LEN, "CONTROL_RELAY");
    ++out->cap_count;
  }
  return true;
}

size_t C2000ppModbusAdapter::getPoints(DevicePoint* out, size_t max) {
  if (!out || max == 0) return 0;
  size_t n = 0;
  memset(&out[n], 0, sizeof(out[n]));
  device_copy(out[n].device_id, sizeof(out[n].device_id), id());
  device_copy(out[n].point_id, sizeof(out[n].point_id), "PP");
  device_copy(out[n].type, sizeof(out[n].type), "loop");
  device_copy(out[n].name, sizeof(out[n].name), "C2000-PP");
  device_copy(out[n].state, sizeof(out[n].state), overlayState());
  ++n;
  if (have_zone1_ && n < max) {
    memset(&out[n], 0, sizeof(out[n]));
    device_copy(out[n].device_id, sizeof(out[n].device_id), id());
    device_copy(out[n].point_id, sizeof(out[n].point_id), "Z1");
    device_copy(out[n].type, sizeof(out[n].type), "zone");
    device_copy(out[n].name, sizeof(out[n].name), "Zone 1");
    device_copy(out[n].state, sizeof(out[n].state), zoneStateName(zone1_code_));
    ++n;
  }
  for (uint8_t i = 0; i < kMaxNamedRelays && n < max; ++i) {
    if (!relay_allow_[i]) continue;
    memset(&out[n], 0, sizeof(out[n]));
    device_copy(out[n].device_id, sizeof(out[n].device_id), id());
    char pid[DEVICE_POINT_ID_MAX];
    snprintf(pid, sizeof(pid), "R%u", (unsigned)(i + 1));
    device_copy(out[n].point_id, sizeof(out[n].point_id), pid);
    device_copy(out[n].type, sizeof(out[n].type), "relay");
    char name[24];
    snprintf(name, sizeof(name), "Relay %u", (unsigned)(i + 1));
    device_copy(out[n].name, sizeof(out[n].name), name);
    device_copy(out[n].state, sizeof(out[n].state),
                relay_state_[i][0] ? relay_state_[i] : "UNKNOWN");
    ++n;
  }
  return n;
}

size_t C2000ppModbusAdapter::getEvents(DeviceEvent* out, size_t max) {
  if (!out || max == 0 || !diag_event_ || !online_) return 0;
  memset(&out[0], 0, sizeof(*out));
  device_copy(out[0].id, sizeof(out[0].id), diag_uuid_);
  device_copy(out[0].device_id, sizeof(out[0].device_id), id());
  device_copy(out[0].point_id, sizeof(out[0].point_id), "PP");
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

bool C2000ppModbusAdapter::execute(const CommandRequest& req, CommandOutcome* out) {
  if (!out) return false;
  out->mesh_ok = true;
  out->device_ok = false;
  out->error[0] = 0;

  if (strcmp(req.capability, "CONTROL_RELAY") != 0) {
    device_copy(out->error, sizeof(out->error), "NO_PERMISSION");
    return false;
  }

  uint8_t n = 0;
  bool on = false;
  if (!parseRelayPoint(req.point_id, &n) || !parseOnOff(req.action, &on) ||
      !relayAllowed(n)) {
    device_copy(out->error, sizeof(out->error), "NO_PERMISSION");
    return false;
  }

  if (tx_blocked_) {
    device_copy(out->error, sizeof(out->error), "NO_PERMISSION");
    return false;
  }
  if (!port_ || !port_->ok()) {
    device_copy(out->error, sizeof(out->error), "HARDWARE");
    return false;
  }

  in_flight_ = false;  // preempt the periodic probe
  const uint16_t coil = (uint16_t)(kRelay1Coil + (n - 1));
  uint8_t adu[8];
  const size_t wn = buildWriteCoil(adu, sizeof(adu), coil, on);
  if (!wn) {
    device_copy(out->error, sizeof(out->error), "HARDWARE");
    return false;
  }
  if (port_->writeFrame(adu, wn) != wn) {
    device_copy(out->error, sizeof(out->error), "HARDWARE");
    return false;
  }

  uint8_t rx[MESH485_FRAME_MAX];
  const size_t got = port_->readFrame(rx, sizeof(rx), kTxnTimeoutMs);
  if (!got || !acceptFc05(rx, got)) {
    device_copy(out->error, sizeof(out->error), "TIMEOUT");
    return false;
  }

  device_copy(relay_state_[n - 1], sizeof(relay_state_[n - 1]), on ? "ON" : "OFF");
  out->device_ok = true;
  return true;
}
