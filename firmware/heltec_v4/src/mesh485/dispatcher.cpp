#include "mesh485/dispatcher.h"

#include "mesh485/base64.h"
#include "mesh485/clock.h"
#include "mesh485/codec.h"
#include "mesh485/roles.h"
#include "storage/event_record.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef MESH485_NATIVE
#include <Arduino.h>
#include <SPIFFS.h>
#endif

static void copyZ(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

static bool startsWith(const char* s, const char* pfx) {
  if (!s || !pfx) return false;
  const size_t n = strlen(pfx);
  return strncmp(s, pfx, n) == 0;
}

static bool isAccessOpen(const char* op) {
  return op && (strcmp(op, "ACCESS_OPEN") == 0 || strcmp(op, "ACCESS_TURNSTILE_OPEN") == 0);
}

static bool mesh485_can_config(Mesh485Role r) { return r == Mesh485Role::Admin; }

static bool jsonAppend(char*& p, const char* end, const char* s) {
  if (!s) return true;
  while (*s) {
    if (p >= end - 1) return false;
    *p++ = *s++;
  }
  *p = 0;
  return true;
}

static bool jsonAppendEscaped(char*& p, const char* end, const char* s) {
  if (!s) return true;
  while (*s) {
    const unsigned char c = (unsigned char)*s++;
    if (c == '"' || c == '\\') {
      if (p + 2 >= end) return false;
      *p++ = '\\';
      *p++ = (char)c;
    } else if (c < 0x20) {
      continue;
    } else {
      if (p >= end - 1) return false;
      *p++ = (char)c;
    }
  }
  *p = 0;
  return true;
}

static bool jsonDeviceSummary(char*& p, const char* end, const DeviceInfo& d, bool comma) {
  if (comma && !jsonAppend(p, end, ",")) return false;
  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"id\":\"%s\",\"protocol\":\"%s\",\"online\":%s}", d.id, d.protocol,
           d.online ? "true" : "false");
  return jsonAppend(p, end, buf);
}

static bool jsonCaps(char*& p, const char* end, const DeviceInfo& d) {
  if (!jsonAppend(p, end, "[")) return false;
  for (uint8_t i = 0; i < d.cap_count; ++i) {
    if (i && !jsonAppend(p, end, ",")) return false;
    if (!jsonAppend(p, end, "\"")) return false;
    if (!jsonAppend(p, end, d.capabilities[i])) return false;
    if (!jsonAppend(p, end, "\"")) return false;
  }
  return jsonAppend(p, end, "]");
}

static bool jsonDeviceFull(char*& p, const char* end, const DeviceInfo& d) {
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"id\":\"%s\",\"vendor\":\"%s\",\"model\":\"%s\",\"protocol\":\"%s\","
           "\"address\":%u,\"online\":%s,\"capabilities\":",
           d.id, d.vendor, d.model, d.protocol, (unsigned)d.address,
           d.online ? "true" : "false");
  if (!jsonAppend(p, end, buf)) return false;
  if (!jsonCaps(p, end, d)) return false;
  return jsonAppend(p, end, "}");
}

static bool jsonPoint(char*& p, const char* end, const DevicePoint& pt, bool comma) {
  if (comma && !jsonAppend(p, end, ",")) return false;
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"device_id\":\"%s\",\"point_id\":\"%s\",\"type\":\"%s\",\"name\":\"%s\","
           "\"state\":\"%s\",\"attributes\":{}}",
           pt.device_id, pt.point_id, pt.type, pt.name, pt.state);
  return jsonAppend(p, end, buf);
}

Mesh485Dispatcher::Mesh485Dispatcher() { devices_.beginAll(); }

void Mesh485Dispatcher::harvestEvents() {
  DeviceEvent ev;
  for (size_t i = 0; i < devices_.count(); ++i) {
    DeviceAdapter* a = devices_.byIndex(i);
    if (!a) continue;
    const size_t n = a->getEvents(&ev, 1);
    if (!n || !ev.id[0] || events_.hasUuid(ev.id)) continue;
    EventRecord rec;
    event_record_clear(&rec);
    event_record_copy_str(rec.uuid, sizeof(rec.uuid), ev.id);
    event_record_copy_str(rec.device_id, sizeof(rec.device_id), ev.device_id);
    event_record_copy_str(rec.point_id, sizeof(rec.point_id), ev.point_id);
    event_record_copy_str(rec.type, sizeof(rec.type), ev.type);
    event_record_copy_str(rec.severity, sizeof(rec.severity), ev.severity);
    event_record_copy_str(rec.state, sizeof(rec.state), ev.state);
    rec.timestamp = ev.timestamp;
    const char* src = ev.source;
    if (src && strcmp(src, "WIEGAND") == 0) src = "RFID";
    event_record_copy_str(rec.source, sizeof(rec.source), src);
    event_record_copy_str(rec.zone_id, sizeof(rec.zone_id), ev.device_id);
    event_record_copy_str(rec.rfid_uid, sizeof(rec.rfid_uid), ev.rfid_uid);
    event_record_copy_str(rec.action, sizeof(rec.action), ev.action);
    event_record_copy_str(rec.status, sizeof(rec.status), ev.status);
    event_record_copy_str(rec.name, sizeof(rec.name), ev.name);
    rec.device_ok = ev.device_ok ? 1 : 0;
    rec.mesh_ok = 0;
    events_.append(rec);
    if (strcmp(rec.type, "PCN") == 0 && rec.state[0]) {
      copyZ(pcn_shown_, sizeof(pcn_shown_), rec.state);
      copyZ(pcn_from_, sizeof(pcn_from_), node_id_);
      if (strcmp(rec.state, "ALARM") == 0) pcn_alarm_gen_++;
    }
  }
}

void Mesh485Dispatcher::setNodeId(const char* id) {
  copyZ(node_id_, sizeof(node_id_), id && id[0] ? id : MESH485_NODE_ID);
}

bool Mesh485Dispatcher::acceptCard(const char* uid, int64_t ts, const char* direction, bool actuate,
                                   bool* opened) {
  if (opened) *opened = false;
  if (!uid || !uid[0]) return false;
  acl_.ensureBegun();
  const AccessDecision d = acl_.checkUid(uid);
  const bool listed = d.result == AccessResult::Allow;
  const char* dir = direction && direction[0] ? direction : "ENTRY";
  devices_.access().noteCard(uid, ts, dir, listed ? "Authorized" : "Denied",
                             listed && d.name[0] ? d.name : "");
  if (listed && actuate) {
    gate_.openPulse(300);
    if (hw_.openPulse) hw_.openPulse(300, hw_.ctx);
    if (opened) *opened = true;
  }
  return true;
}

bool Mesh485Dispatcher::pushRdm(const uint8_t* data, size_t len, uint32_t now_ms,
                                const char* direction, bool actuate, bool* opened) {
  if (opened) *opened = false;
  if (data && len) rdm_.push(data, len, now_ms);
  char uid[16];
  if (!rdm_.take(uid, sizeof(uid))) return false;
  if (rdm_ignore_) return false;
  return acceptCard(uid, (int64_t)(now_ms / 1000u), direction, actuate, opened);
}

static bool isPendingPcn(const EventRecord& e) {
  return strcmp(e.type, "PCN") == 0 && !e.mesh_ok && e.uuid[0];
}

size_t Mesh485Dispatcher::pollPcnHop(uint8_t* out, size_t max, uint32_t now_ms) {
  if (!out || max < kPcnHopBytes) return 0;
  if (!role_source_) return 0;
  EventRecord e;
  bool found = false;
  for (int i = 0; i < events_.count(); ++i) {
    EventRecord row;
    if (!events_.getAt((size_t)i, &row)) break;
    if (!isPendingPcn(row)) continue;
    e = row;
    found = true;
    break;
  }
  const bool backoff = found && hop_wait_uuid_[0] && strcmp(hop_wait_uuid_, e.uuid) == 0 &&
                       (int32_t)(hop_next_ms_ - now_ms) > 0;
  if (found && !backoff) {
    // now_ms is inside the frame so each send encrypts differently. MeshCore
    // drops a byte-identical flood, which froze the first node's line.
    const size_t n = pcn_hop_encode_state(out, max, node_id_, e.uuid, e.state, now_ms, channel_,
                                         alarm_name_);
    if (n) {
      uint32_t pause = 1000;
      for (uint8_t i = 0; i < e.mesh_retries && i < 4; ++i) pause *= 2;
      hop_next_ms_ = now_ms + pause;
      copyZ(hop_wait_uuid_, sizeof(hop_wait_uuid_), e.uuid);
      events_.failMesh(e.sequence);
      if (devices_.pcn().episodeAlarm()) beacon_next_ms_ = now_ms + kPcnBeaconMs;
      return n;
    }
  }
  // Repeat the same ALARM uuid every 15 s while the contact stays closed.
  // A contact that is already closed at boot is not an episode and stays quiet.
  if (!devices_.pcn().episodeAlarm()) {
    beacon_next_ms_ = 0;
    return 0;
  }
  if (beacon_next_ms_ == 0) beacon_next_ms_ = now_ms + kPcnBeaconMs;
  if ((int32_t)(now_ms - beacon_next_ms_) < 0) return 0;
  const char* uuid = devices_.pcn().episodeUuid();
  if (!uuid || !uuid[0]) return 0;
  beacon_next_ms_ = now_ms + kPcnBeaconMs;
  return pcn_hop_encode_state(out, max, node_id_, uuid, "ALARM", now_ms, channel_, alarm_name_);
}

bool Mesh485Dispatcher::acceptPcnFrame(const uint8_t* frame, size_t len, uint8_t* ack,
                                       size_t* ack_len) {
  if (ack_len) *ack_len = 0;
  PcnHopView view;
  if (!pcn_hop_decode(frame, len, &view)) return false;
  if (view.kind == kPcnHopAck) {
    for (int i = 0; i < events_.count(); ++i) {
      EventRecord row;
      if (!events_.getAt((size_t)i, &row)) break;
      if (strcmp(row.uuid, view.uuid) == 0) {
        events_.ackMesh(row.sequence);
        break;
      }
    }
    return true;
  }
  if (strcmp(view.node, node_id_) == 0) return true;
#ifdef MESH485_DEBUG
  Serial.printf("PCN hop RX %s\n", view.state);
#endif
  const bool alarm = strcmp(view.state, "ALARM") == 0;
  if (!role_source_ && view.channel != channel_) return true;
  if (!role_source_ && alarm && noteHold(view.node, mesh485_now_ms(), view.label)) {
#ifndef MESH485_NATIVE
    Serial.printf("PCN %s %s ch %u\n", view.node, view.label[0] ? view.label : view.node,
                  (unsigned)channel_);
#endif
  }
  if (!events_.hasUuid(view.uuid)) {
    EventRecord rec;
    event_record_clear(&rec);
    event_record_copy_str(rec.uuid, sizeof(rec.uuid), view.uuid);
    event_record_copy_str(rec.device_id, sizeof(rec.device_id), "PCN-001");
    event_record_copy_str(rec.point_id, sizeof(rec.point_id), "ALARM");
    event_record_copy_str(rec.type, sizeof(rec.type), "PCN");
    event_record_copy_str(rec.severity, sizeof(rec.severity), alarm ? "ALARM" : "INFO");
    event_record_copy_str(rec.state, sizeof(rec.state), alarm ? "ALARM" : "NORMAL");
    event_record_copy_str(rec.source, sizeof(rec.source), "LORA");
    event_record_copy_str(rec.zone_id, sizeof(rec.zone_id), "PCN-001");
    rec.sequence = view.sequence;
    rec.device_ok = 1;
    rec.mesh_ok = 1;
    events_.append(rec, true);
    // A source journals the frame but does not raise its own alarm from it.
    if (alarm && !role_source_) pcn_alarm_gen_++;
  }
  if (role_source_ && view.state[0]) {
    copyZ(pcn_shown_, sizeof(pcn_shown_), view.state);
    if (view.node[0]) copyZ(pcn_from_, sizeof(pcn_from_), view.node);
  } else if (!role_source_ && view.node[0]) {
    copyZ(pcn_from_, sizeof(pcn_from_), view.node);
  }
  if (ack && ack_len) {
    const size_t n = pcn_hop_encode_ack(ack, 80, view.uuid);
    *ack_len = n;
  }
  return true;
}

void Mesh485Dispatcher::injectStubCard() {
  EventRecord rec;
  event_record_clear(&rec);
  event_record_copy_str(rec.device_id, sizeof(rec.device_id), "TS-001");
  event_record_copy_str(rec.point_id, sizeof(rec.point_id), "READER");
  event_record_copy_str(rec.type, sizeof(rec.type), "CARD");
  event_record_copy_str(rec.severity, sizeof(rec.severity), "INFO");
  event_record_copy_str(rec.state, sizeof(rec.state), "SEEN");
  event_record_copy_str(rec.source, sizeof(rec.source), "RFID");
  event_record_copy_str(rec.zone_id, sizeof(rec.zone_id), "TS-001");
  event_record_copy_str(rec.rfid_uid, sizeof(rec.rfid_uid), "STUBCARD");
  event_record_copy_str(rec.action, sizeof(rec.action), "ENTRY");
  event_record_copy_str(rec.status, sizeof(rec.status), "Authorized");
  event_record_copy_str(rec.remarks, sizeof(rec.remarks), "usb stub");
  rec.device_ok = 1;
  rec.timestamp = 0;
  events_.append(rec);
}

bool Mesh485Dispatcher::sinceMatch(const EventRecord& e, const char* since, bool* include) {
  if (!since || !since[0] || strcmp(since, "0") == 0) return true;
  bool digits = true;
  for (const char* p = since; *p; ++p) {
    if (*p < '0' || *p > '9') {
      digits = false;
      break;
    }
  }
  if (digits) return e.sequence > (uint32_t)atoi(since);
  if (!include) return strcmp(e.uuid, since) != 0;
  if (*include) return true;
  if (strcmp(e.uuid, since) == 0) {
    *include = true;
    return false;
  }
  return false;
}

size_t Mesh485Dispatcher::nextPendingEventLine(char* out, size_t max, uint32_t* sequence) {
  if (!subscribed_ || !out || max < 32) return 0;
  for (int i = 0; i < events_.count(); ++i) {
    EventRecord e;
    if (!events_.getAt((size_t)i, &e)) break;
    if (e.mesh_ok || e.mesh_retries >= EventStore::kMaxMeshRetries) continue;
    if (strcmp(e.type, "PCN") == 0) continue;
    if (sequence) *sequence = e.sequence;
    e.mesh_ok = 1;
    return event_record_ndjson(e, node_id_[0] ? node_id_ : MESH485_NODE_ID, out, max);
  }
  return 0;
}

bool Mesh485Dispatcher::ackMesh(uint32_t sequence) { return events_.ackMesh(sequence); }

bool Mesh485Dispatcher::failMesh(uint32_t sequence) { return events_.failMesh(sequence); }

void Mesh485Dispatcher::opEventSubscribe(const Mesh485Request& req, Mesh485Response* resp) {
  if (strcmp(req.stub, "CARD") == 0) injectStubCard();
  subscribed_ = true;
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"subscribed\":true,\"pending\":%d,\"pipe\":\"usb\"}", events_.pendingCount());
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opCommandExecute(const Mesh485Request& req, const Mesh485Env& env,
                                         Mesh485Response* resp) {
  const char* cap = req.capability;
  const char* did = req.device_id;
  const char* pid = req.point_id;

  auto finish = [&](const char* err, bool mesh_ok, bool device_ok) {
    last_cmd_.used = true;
    copyZ(last_cmd_.optr, sizeof(last_cmd_.optr), req.optr);
    copyZ(last_cmd_.device_id, sizeof(last_cmd_.device_id), did);
    copyZ(last_cmd_.capability, sizeof(last_cmd_.capability), cap);
    copyZ(last_cmd_.point_id, sizeof(last_cmd_.point_id), pid);
    last_cmd_.mesh_ok = mesh_ok;
    last_cmd_.device_ok = device_ok;
    copyZ(last_cmd_.error, sizeof(last_cmd_.error), err ? err : "");
    last_txn_mesh_ok_ = mesh_ok;
    last_txn_device_ok_ = device_ok;

    char err_json[32];
    if (err && err[0]) {
      snprintf(err_json, sizeof(err_json), "\"%s\"", err);
    } else {
      copyZ(err_json, sizeof(err_json), "null");
    }
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"device_id\":\"%s\",\"capability\":\"%s\",\"point_id\":\"%s\","
             "\"mesh_ok\":%s,\"device_ok\":%s,\"error\":%s}",
             did[0] ? did : "", cap[0] ? cap : "", pid[0] ? pid : "",
             mesh_ok ? "true" : "false", device_ok ? "true" : "false", err_json);
    if (err && err[0]) {
      setError(err);
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), err);
    } else {
      setError("");
      copyZ(resp->result, sizeof(resp->result), "OK");
    }
  };

  if (!mesh485_can_execute(caller_)) {
    finish("NO_PERMISSION", true, false);
    return;
  }
  if (mesh485_is_forbidden_cap(cap) || !mesh485_is_executable_cap(cap)) {
    finish("NO_PERMISSION", true, false);
    return;
  }
  if (!did[0]) {
    finish("UNKNOWN_CMD", true, false);
    return;
  }
  DeviceAdapter* a = devices_.byId(did);
  if (!a) {
    refuseUnknownDevice(resp);
    return;
  }
  DeviceInfo info;
  if (!a->getInfo(&info) || !device_has_cap(info, cap)) {
    finish("NO_PERMISSION", true, false);
    return;
  }

  CommandRequest creq;
  memset(&creq, 0, sizeof(creq));
  device_copy(creq.capability, sizeof(creq.capability), cap);
  device_copy(creq.point_id, sizeof(creq.point_id), pid);
  device_copy(creq.action, sizeof(creq.action), req.action);
  CommandOutcome outcome;
  a->execute(creq, &outcome);
  finish(outcome.error, outcome.mesh_ok, outcome.device_ok);

  EventRecord rec;
  event_record_clear(&rec);
  event_record_copy_str(rec.device_id, sizeof(rec.device_id), did);
  event_record_copy_str(rec.point_id, sizeof(rec.point_id), pid);
  event_record_copy_str(rec.type, sizeof(rec.type), "CONTROL");
  event_record_copy_str(rec.severity, sizeof(rec.severity),
                        outcome.device_ok ? "INFO" : "WARN");
  event_record_copy_str(rec.state, sizeof(rec.state),
                        outcome.device_ok ? "ACK" : "FAIL");
  event_record_copy_str(rec.source, sizeof(rec.source), "USB");
  event_record_copy_str(rec.action, sizeof(rec.action), req.action);
  event_record_copy_str(rec.status, sizeof(rec.status),
                        outcome.error[0] ? outcome.error : "OK");
  event_record_copy_str(rec.name, sizeof(rec.name), req.optr);
  rec.device_ok = outcome.device_ok ? 1 : 0;
  rec.mesh_ok = 0;
  rec.timestamp = env.unix_now ? env.unix_now : (int64_t)env.uptime_s;
  events_.append(rec);
}

void Mesh485Dispatcher::finishAccess(Mesh485Response* resp, const char* err, bool mesh_ok,
                                     bool device_ok, const char* extra_json) {
  last_txn_mesh_ok_ = mesh_ok;
  last_txn_device_ok_ = device_ok;
  char err_json[32];
  if (err && err[0]) {
    snprintf(err_json, sizeof(err_json), "\"%s\"", err);
  } else {
    copyZ(err_json, sizeof(err_json), "null");
  }
  if (extra_json && extra_json[0]) {
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"mesh_ok\":%s,\"device_ok\":%s,\"error\":%s,%s", mesh_ok ? "true" : "false",
             device_ok ? "true" : "false", err_json, extra_json);
  } else {
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"mesh_ok\":%s,\"device_ok\":%s,\"error\":%s}", mesh_ok ? "true" : "false",
             device_ok ? "true" : "false", err_json);
  }
  if (err && err[0]) {
    setError(err);
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), err);
  } else {
    setError("");
    copyZ(resp->result, sizeof(resp->result), "OK");
  }
}

void Mesh485Dispatcher::opAccess(const Mesh485Request& req, const Mesh485Env& env,
                                 Mesh485Response* resp) {
  acl_.ensureBegun();
  const Mesh485Role role = caller_;
  const bool can_open = mesh485_can_execute(role);
  const bool can_admin = role == Mesh485Role::Admin;
  const char* src = env.source && env.source[0] ? env.source : "USB";

  auto deny = [&]() {
    finishAccess(resp, "NO_PERMISSION", true, false, nullptr);
  };

  if (strcmp(req.op, "ACCESS_STATUS") == 0) {
    const char* st = gate_.stateName();
    devices_.access().setGateState(st);
    const char* last = devices_.access().lastUid();
    if (!last || !last[0]) last = acl_.lastUid();
    char extra[192];
    if (last && last[0]) {
      snprintf(extra, sizeof(extra),
               "\"device\":\"TS-001\",\"state\":\"%s\",\"users\":%u,\"last_uid\":\"%s\"}", st,
               (unsigned)acl_.enabledCount(), last);
    } else {
      snprintf(extra, sizeof(extra), "\"device\":\"TS-001\",\"state\":\"%s\",\"users\":%u}", st,
               (unsigned)acl_.enabledCount());
    }
    finishAccess(resp, "", true, true, extra);
    return;
  }

  if (isAccessOpen(req.op)) {
    if (!can_open) {
      deny();
      return;
    }
    uint16_t pulse = req.pulse_ms ? req.pulse_ms : 300;
    gate_.openPulse(pulse);
    gate_.noteRemote();
    if (hw_.openPulse) hw_.openPulse(pulse, hw_.ctx);
    if (hw_.showRemote) hw_.showRemote("OPEN", req.optr, hw_.ctx);
    devices_.access().setGateState(gate_.stateName());
    const bool device_ok = gate_.lastOpenOk();
    last_cmd_.used = true;
    copyZ(last_cmd_.optr, sizeof(last_cmd_.optr), req.optr);
    copyZ(last_cmd_.device_id, sizeof(last_cmd_.device_id), "TS-001");
    copyZ(last_cmd_.capability, sizeof(last_cmd_.capability), "ACCESS_OPEN");
    copyZ(last_cmd_.point_id, sizeof(last_cmd_.point_id), "TURNSTILE");
    last_cmd_.mesh_ok = true;
    last_cmd_.device_ok = device_ok;
    copyZ(last_cmd_.error, sizeof(last_cmd_.error), device_ok ? "" : "HARDWARE");

    EventRecord rec;
    event_record_clear(&rec);
    event_record_copy_str(rec.device_id, sizeof(rec.device_id), "TS-001");
    event_record_copy_str(rec.point_id, sizeof(rec.point_id), "TURNSTILE");
    event_record_copy_str(rec.type, sizeof(rec.type), "REMOTE_OPEN");
    event_record_copy_str(rec.severity, sizeof(rec.severity), device_ok ? "INFO" : "WARN");
    event_record_copy_str(rec.state, sizeof(rec.state), device_ok ? "OPEN" : "FAIL");
    event_record_copy_str(rec.source, sizeof(rec.source), src);
    event_record_copy_str(rec.action, sizeof(rec.action), "REMOTE_OPEN");
    event_record_copy_str(rec.status, sizeof(rec.status), device_ok ? "Authorized" : "HARDWARE");
    event_record_copy_str(rec.name, sizeof(rec.name), req.optr);
    rec.device_ok = device_ok ? 1 : 0;
    rec.mesh_ok = 0;
    rec.timestamp = env.unix_now ? env.unix_now : (int64_t)env.uptime_s;
    events_.append(rec);

    char extra[96];
    snprintf(extra, sizeof(extra), "\"device\":\"TS-001\",\"state\":\"%s\"}", gate_.stateName());
    finishAccess(resp, device_ok ? "" : "HARDWARE", true, device_ok, extra);
    return;
  }

  if (strcmp(req.op, "ACCESS_LOCK") == 0) {
    if (!can_open) {
      deny();
      return;
    }
    gate_.lock();
    if (hw_.lock) hw_.lock(hw_.ctx);
    devices_.access().setGateState(gate_.stateName());
    const bool device_ok = gate_.lastLockOk();
    EventRecord rec;
    event_record_clear(&rec);
    event_record_copy_str(rec.device_id, sizeof(rec.device_id), "TS-001");
    event_record_copy_str(rec.point_id, sizeof(rec.point_id), "TURNSTILE");
    event_record_copy_str(rec.type, sizeof(rec.type), "REMOTE_LOCK");
    event_record_copy_str(rec.source, sizeof(rec.source), src);
    event_record_copy_str(rec.action, sizeof(rec.action), "REMOTE_LOCK");
    event_record_copy_str(rec.status, sizeof(rec.status), device_ok ? "OK" : "HARDWARE");
    event_record_copy_str(rec.name, sizeof(rec.name), req.optr);
    rec.device_ok = device_ok ? 1 : 0;
    rec.mesh_ok = 0;
    rec.timestamp = env.unix_now ? env.unix_now : (int64_t)env.uptime_s;
    events_.append(rec);
    char extra[96];
    snprintf(extra, sizeof(extra), "\"device\":\"TS-001\",\"state\":\"%s\"}", gate_.stateName());
    finishAccess(resp, device_ok ? "" : "HARDWARE", true, device_ok, extra);
    return;
  }

  if (strcmp(req.op, "ACCESS_GET_EVENTS") == 0) {
    Mesh485Request filtered = req;
    if (!filtered.device_id[0]) copyZ(filtered.device_id, sizeof(filtered.device_id), "TS-001");
    opEventList(filtered, resp);
    return;
  }

  if (strcmp(req.op, "ACCESS_GET_USERS") == 0) {
    if (!can_admin) {
      deny();
      return;
    }
    uint16_t limit = req.list_limit ? req.list_limit : 16;
    if (limit > 16) limit = 16;
    const uint16_t offset = req.list_offset;
    const uint16_t total = acl_.count();
    char* p = resp->payload;
    const char* end = resp->payload + sizeof(resp->payload);
    char head[96];
    snprintf(head, sizeof(head),
             "{\"mesh_ok\":true,\"device_ok\":true,\"error\":null,\"total\":%u,\"offset\":%u,"
             "\"users\":[",
             (unsigned)total, (unsigned)offset);
    if (!jsonAppend(p, end, head)) return;
    uint16_t n = 0;
    for (uint16_t i = offset; i < total && n < limit; ++i) {
      const AccessAclRecord* r = acl_.recordAt(i);
      if (!r) break;
      const size_t need = 48 + strlen(r->uid) + strlen(r->employee_id) + strlen(r->name);
      if ((size_t)(end - p) < need + 16) break;
      if (n && !jsonAppend(p, end, ",")) break;
      if (!jsonAppend(p, end, "{\"uid\":\"")) break;
      if (!jsonAppendEscaped(p, end, r->uid)) break;
      if (!jsonAppend(p, end, "\",\"enabled\":")) break;
      if (!jsonAppend(p, end, r->enabled ? "true" : "false")) break;
      if (r->employee_id[0]) {
        if (!jsonAppend(p, end, ",\"employee_id\":\"")) break;
        if (!jsonAppendEscaped(p, end, r->employee_id)) break;
        if (!jsonAppend(p, end, "\"")) break;
      }
      if (r->name[0]) {
        if (!jsonAppend(p, end, ",\"name\":\"")) break;
        if (!jsonAppendEscaped(p, end, r->name)) break;
        if (!jsonAppend(p, end, "\"")) break;
      }
      if (!jsonAppend(p, end, "}")) break;
      ++n;
    }
    if (!jsonAppend(p, end, "]}")) return;
    setError("");
    copyZ(resp->result, sizeof(resp->result), "OK");
    return;
  }

  if (strcmp(req.op, "ACCESS_ADD_CARD") == 0) {
    if (!can_admin) {
      deny();
      return;
    }
    if (!req.uid[0]) {
      finishAccess(resp, "UNKNOWN_CMD", true, false, nullptr);
      return;
    }
    const bool en = req.uid_enabled_set ? (req.uid_enabled != 0) : true;
    if (!acl_.addCard(req.uid, req.employee_id, req.card_name, en)) {
      finishAccess(resp, "BUSY", true, false, "\"reason\":\"ACL_FULL\"}");
      return;
    }
    char extra[96];
    snprintf(extra, sizeof(extra), "\"users\":%u,\"device\":\"TS-001\"}",
             (unsigned)acl_.enabledCount());
    finishAccess(resp, "", true, true, extra);
    return;
  }

  if (strcmp(req.op, "ACCESS_REMOVE_CARD") == 0) {
    if (!can_admin) {
      deny();
      return;
    }
    if (!acl_.removeCard(req.uid)) {
      finishAccess(resp, "UNKNOWN_CMD", true, false, "\"reason\":\"unknown uid\"}");
      return;
    }
    char extra[96];
    snprintf(extra, sizeof(extra), "\"users\":%u,\"device\":\"TS-001\"}",
             (unsigned)acl_.enabledCount());
    finishAccess(resp, "", true, true, extra);
    return;
  }

  copyZ(resp->result, sizeof(resp->result), "ERR");
  copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
  copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"UNKNOWN_CMD\"}");
}

void Mesh485Dispatcher::setError(const char* e) {
  copyZ(last_error_, sizeof(last_error_), e ? e : "");
}

void Mesh485Dispatcher::refuseUnknownDevice(Mesh485Response* resp) {
  setError("UNKNOWN_CMD");
  copyZ(resp->result, sizeof(resp->result), "ERR");
  copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
  copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown device_id\"}");
}

void Mesh485Dispatcher::refuseBadAuth(Mesh485Response* resp) {
  setError("BAD_AUTH");
  copyZ(resp->result, sizeof(resp->result), "ERR");
  copyZ(resp->error, sizeof(resp->error), "BAD_AUTH");
  copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"BAD_AUTH\"}");
}

void Mesh485Dispatcher::opAuthSet(const Mesh485Request& req, Mesh485Response* resp) {
  if (caller_ != Mesh485Role::Admin) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "NO_PERMISSION");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"NO_PERMISSION\"}");
    return;
  }
  const char* name = req.card_name[0] ? req.card_name : "";
  if (!name[0]) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"missing name\"}");
    return;
  }
  if (req.uid_enabled_set && req.uid_enabled == 0) {
    Mesh485Role existing;
    if (!auth_.lookup(name, &existing)) {
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
      copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown name\"}");
      return;
    }
    if (existing == Mesh485Role::Admin && auth_.adminCount() <= 1) {
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), "BUSY");
      copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"last admin\"}");
      return;
    }
    auth_.remove(name);
    auth_.persist();
    setError("");
    copyZ(resp->result, sizeof(resp->result), "OK");
    snprintf(resp->payload, sizeof(resp->payload), "{\"operators\":%u}",
             (unsigned)auth_.count());
    return;
  }
  Mesh485Role role;
  const bool have_role = req.role[0] != 0;
  if (have_role) {
    if (!mesh485_parse_role(req.role, &role)) {
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
      copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown role\"}");
      return;
    }
    if (!auth_.set(name, role)) {
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), "BUSY");
      copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"AUTH_FULL\"}");
      return;
    }
  } else if (!req.pubkey_set) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown role\"}");
    return;
  } else if (!auth_.lookup(name, nullptr)) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown name\"}");
    return;
  }
  if (req.pubkey_set) {
    if (!req.pubkey[0]) {
      if (!auth_.clearPubkey(name)) {
        copyZ(resp->result, sizeof(resp->result), "ERR");
        copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
        copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown name\"}");
        return;
      }
    } else {
      uint8_t pub[MESH485_PUBKEY_LEN];
      if (!mesh485_parse_pubkey_hex(req.pubkey, pub)) {
        copyZ(resp->result, sizeof(resp->result), "ERR");
        copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
        copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"bad pubkey\"}");
        return;
      }
      if (!auth_.setPubkey(name, pub)) {
        copyZ(resp->result, sizeof(resp->result), "ERR");
        copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
        copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown name\"}");
        return;
      }
    }
  }
  auth_.persist();
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"name\":\"%s\",\"operators\":%u}", name, (unsigned)auth_.count());
}

DeviceAdapter* Mesh485Dispatcher::resolveDevice(const Mesh485Request& req, bool required) {
  if (req.device_id[0]) {
    DeviceAdapter* a = devices_.byId(req.device_id);
    if (!a && required) return nullptr;
    return a;
  }
  return required ? devices_.first() : nullptr;
}

void Mesh485Dispatcher::fillMeta(const Mesh485Request& req, const Mesh485Env& env,
                                Mesh485Response* resp) {
  copyZ(resp->id, sizeof(resp->id), req.id);
  copyZ(resp->node, sizeof(resp->node), env.node_id ? env.node_id : MESH485_NODE_ID);
  copyZ(resp->op, sizeof(resp->op), req.op);
  if (env.unix_now > 0) resp->ts = env.unix_now;
  else if (req.ts > 0) resp->ts = req.ts;
  else resp->ts = (int64_t)env.uptime_s;
}

void Mesh485Dispatcher::opNodeStatus(const Mesh485Env& env, Mesh485Response* resp) {
  const Mesh485Port* p = env.port;
  const bool ok = p ? p->ok() : false;
  const uint32_t baud = p ? p->baud() : 9600;
  const uint32_t rx = p ? p->rxCount() : 0;
  const uint32_t tx = p ? p->txCount() : 0;
  char session[96];
  if (bridge_.isOpen()) {
    snprintf(session, sizeof(session), "{\"level\":\"%s\",\"idle_s\":%lu}",
             bridge_.levelName(), (unsigned long)bridge_.idleRemaining(env.uptime_s));
  } else {
    copyZ(session, sizeof(session), "\"off\"");
  }
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"radio\":\"%s\",\"repeater\":%s,\"uptime_s\":%lu,\"profile\":\"access+rs485\","
           "\"rs485\":{\"ok\":%s,\"baud\":%lu,\"protocol\":\"%s\",\"rx\":%lu,\"tx\":%lu},"
           "\"session\":%s,\"devices\":%u}",
           env.radio_ok ? "ok" : "error", env.repeater ? "true" : "false",
           (unsigned long)env.uptime_s, ok ? "true" : "false", (unsigned long)baud,
           devices_.busProtocol(), (unsigned long)rx, (unsigned long)tx, session,
           (unsigned)devices_.count());
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opConfigGet(const Mesh485Env& env, Mesh485Response* resp) {
  const Mesh485Port* p = env.port;
  const uint32_t baud = p ? p->baud() : 9600;
  const char par = p ? p->parity() : 'N';
  const uint8_t stop = p ? p->stopBits() : 1;
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"baud\":%lu,\"parity\":\"%c\",\"stop\":%u,\"protocol\":\"%s\","
           "\"protocols\":[\"RAW\",\"BOLID_C2000PP\",\"BOLID_SIGNAL10\",\"BOLID_SIGNAL20\"],"
           "\"mode\":\"%s\"}",
           (unsigned long)baud, par, (unsigned)stop, devices_.busProtocol(),
           bridge_.isOpen() ? "BRIDGE" : "MONITOR");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opConfigSet(const Mesh485Request& req, const Mesh485Env& env,
                                    Mesh485Response* resp) {
  if (!mesh485_can_config(caller_)) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "NO_PERMISSION");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"NO_PERMISSION\"}");
    return;
  }
  if (!req.protocol[0]) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "NO_PERMISSION");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"persist refused\"}");
    return;
  }
  if (strcmp(req.protocol, "AUTO") == 0) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"no AUTO\"}");
    return;
  }
  if (!devices_.setBusProtocol(req.protocol)) {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown protocol\"}");
    return;
  }
  cfg_.setAndPersist(req.protocol);
  opConfigGet(env, resp);
}

void Mesh485Dispatcher::opDiagnostics(const Mesh485Env& env, Mesh485Response* resp) {
  const Mesh485Port* p = env.port;
  const uint32_t rx = p ? p->rxCount() : 0;
  const uint32_t tx = p ? p->txCount() : 0;
  char err[48];
  if (last_error_[0]) {
    snprintf(err, sizeof(err), "\"%s\"", last_error_);
  } else {
    copyZ(err, sizeof(err), "null");
  }
  char last_sess[96];
  const Mesh485LastSession& ls = bridge_.lastSession();
  if (ls.used) {
    snprintf(last_sess, sizeof(last_sess),
             "{\"operator\":\"%s\",\"ts\":%lld,\"level\":\"%s\"}", ls.optr,
             (long long)ls.ts, ls.level);
  } else {
    copyZ(last_sess, sizeof(last_sess), "null");
  }
  char snr[16];
  snprintf(snr, sizeof(snr), "%.1f", (double)env.snr);
  bool ev_mesh = false;
  bool ev_dev = false;
  events_.lastFlags(&ev_mesh, &ev_dev);
  char last_cmd[160];
  if (last_cmd_.used) {
    snprintf(last_cmd, sizeof(last_cmd),
             "{\"operator\":\"%s\",\"device\":\"%s\",\"cap\":\"%s\",\"point\":\"%s\","
             "\"mesh_ok\":%s,\"device_ok\":%s,\"error\":\"%s\"}",
             last_cmd_.optr, last_cmd_.device_id, last_cmd_.capability, last_cmd_.point_id,
             last_cmd_.mesh_ok ? "true" : "false", last_cmd_.device_ok ? "true" : "false",
             last_cmd_.error[0] ? last_cmd_.error : "");
  } else {
    copyZ(last_cmd, sizeof(last_cmd), "null");
  }
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"rssi\":%d,\"snr\":%s,\"rx\":%lu,\"tx\":%lu,\"rs485_rx\":%lu,\"rs485_tx\":%lu,"
           "\"last_error\":%s,\"heap\":%lu,\"last_session\":%s,\"last_command\":%s,"
           "\"mesh_ok\":%s,\"device_ok\":%s,"
           "\"events\":{\"queued\":%d,\"pending_mesh\":%d,\"last_mesh_ok\":%s,\"last_device_ok\":%s},"
           "\"rdm_err\":%lu}",
           env.rssi, snr, (unsigned long)env.mesh_rx, (unsigned long)env.mesh_tx,
           (unsigned long)rx, (unsigned long)tx, err, (unsigned long)env.heap, last_sess,
           last_cmd, env.radio_ok ? "true" : "false", last_txn_device_ok_ ? "true" : "false",
           events_.count(), events_.pendingCount(), ev_mesh ? "true" : "false",
           ev_dev ? "true" : "false", (unsigned long)rdm_.errorCount());
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opRs485Transaction(const Mesh485Request& req, const Mesh485Env& env,
                                           Mesh485Response* resp) {
  if (bridge_.isOpen() && !bridge_.allowTransaction()) {
    setError("NO_PERMISSION");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "NO_PERMISSION");
    copyZ(resp->payload, sizeof(resp->payload),
          "{\"mesh_ok\":true,\"tx_ok\":false,\"device_ok\":false,\"frame\":\"\","
          "\"error\":\"NO_PERMISSION\"}");
    return;
  }

  uint8_t raw[MESH485_FRAME_MAX];
  size_t raw_len = 0;
  if (req.frame_b64[0]) {
    raw_len = mesh485_b64_decode(req.frame_b64, raw, sizeof(raw));
    if (raw_len == 0) {
      setError("HARDWARE");
      copyZ(resp->result, sizeof(resp->result), "ERR");
      copyZ(resp->error, sizeof(resp->error), "HARDWARE");
      copyZ(resp->payload, sizeof(resp->payload),
            "{\"mesh_ok\":true,\"tx_ok\":false,\"device_ok\":false,\"frame\":\"\",\"error\":\"HARDWARE\"}");
      return;
    }
  }

  if (!env.port || !env.port->ok()) {
    setError("HARDWARE");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "HARDWARE");
    copyZ(resp->payload, sizeof(resp->payload),
          "{\"mesh_ok\":true,\"tx_ok\":false,\"device_ok\":false,\"frame\":\"\",\"error\":\"HARDWARE\"}");
    return;
  }

  uint32_t timeout = req.timeout_ms ? req.timeout_ms : MESH485_TXN_TIMEOUT_DEFAULT_MS;
  if (timeout > MESH485_TXN_TIMEOUT_MAX_MS) timeout = MESH485_TXN_TIMEOUT_MAX_MS;

  bool tx_ok = true;
  if (raw_len > 0) {
    const size_t wrote = env.port->writeFrame(raw, raw_len);
    tx_ok = wrote == raw_len;
  } else {
    tx_ok = false;
  }

  uint8_t rx[MESH485_FRAME_MAX];
  const size_t got = env.port->readFrame(rx, sizeof(rx), timeout);
  const bool device_ok = got > 0;

  char frame_b64[MESH485_B64_MAX];
  frame_b64[0] = 0;
  if (got) mesh485_b64_encode(rx, got, frame_b64, sizeof(frame_b64));

  if (bridge_.allowTransaction()) bridge_.touch(env.uptime_s);
  if (env.port) env.port->setDirection(false);

  if (!tx_ok) {
    setError("HARDWARE");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "HARDWARE");
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"mesh_ok\":true,\"tx_ok\":false,\"device_ok\":%s,\"frame\":\"%s\",\"error\":\"HARDWARE\"}",
             device_ok ? "true" : "false", frame_b64);
    return;
  }

  if (!device_ok) {
    last_txn_mesh_ok_ = true;
    last_txn_device_ok_ = false;
    setError("TIMEOUT");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "TIMEOUT");
    copyZ(resp->payload, sizeof(resp->payload),
          "{\"mesh_ok\":true,\"tx_ok\":true,\"device_ok\":false,\"frame\":\"\",\"error\":\"TIMEOUT\"}");
    return;
  }

  last_txn_mesh_ok_ = true;
  last_txn_device_ok_ = true;
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"mesh_ok\":true,\"tx_ok\":true,\"device_ok\":true,\"frame\":\"%s\",\"error\":null}",
           frame_b64);
}

void Mesh485Dispatcher::opBridgeOpen(const Mesh485Request& req, const Mesh485Env& env,
                                     Mesh485Response* resp) {
  const Mesh485BridgeLevel lv = Mesh485Bridge::parseLevel(req.level);
  if (lv == Mesh485BridgeLevel::Unknown) {
    setError("UNKNOWN_CMD");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
    copyZ(resp->payload, sizeof(resp->payload), "{\"session\":false}");
    return;
  }
  if (lv == Mesh485BridgeLevel::Control && !MESH485_ALLOW_BRIDGE_CONTROL) {
    setError("NO_PERMISSION");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "NO_PERMISSION");
    copyZ(resp->payload, sizeof(resp->payload), "{\"session\":false}");
    return;
  }
  if (bridge_.isOpen()) {
    setError("BUSY");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "BUSY");
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"session\":true,\"level\":\"%s\"}", bridge_.levelName());
    return;
  }

  uint32_t idle = req.idle_s;
  if (!bridge_.open(lv, idle, req.optr, req.ts, env.uptime_s, env.port)) {
    setError("HARDWARE");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "HARDWARE");
    copyZ(resp->payload, sizeof(resp->payload), "{\"session\":false}");
    return;
  }

  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"session\":true,\"level\":\"%s\",\"port\":\"RS485\",\"idle_s\":%lu}",
           bridge_.levelName(), (unsigned long)bridge_.idleS());
}

void Mesh485Dispatcher::opBridgeClose(const Mesh485Env& env, Mesh485Response* resp) {
  (void)env;
  bridge_.close();
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
  copyZ(resp->payload, sizeof(resp->payload),
        "{\"session\":false,\"port\":\"RS485\"}");
}

void Mesh485Dispatcher::opBridgePoll(const Mesh485Env& env, Mesh485Response* resp) {
  if (bridge_.isOpen()) bridge_.touch(env.uptime_s);
  bridge_.drainPayload(resp->payload, sizeof(resp->payload));
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opDeviceDiscover(Mesh485Response* resp) {
  char* p = resp->payload;
  const char* end = resp->payload + sizeof(resp->payload);
  if (!jsonAppend(p, end, "{\"devices\":[")) return;
  bool first = true;
  for (size_t i = 0; i < devices_.count(); ++i) {
    DeviceAdapter* a = devices_.byIndex(i);
    if (!a) continue;
    DeviceInfo info;
    if (!a->getInfo(&info)) continue;
    if (!jsonDeviceSummary(p, end, info, !first)) return;
    first = false;
  }
  if (!jsonAppend(p, end, "]}")) return;
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opDeviceInfo(const Mesh485Request& req, Mesh485Response* resp) {
  DeviceAdapter* a = resolveDevice(req, true);
  if (!a) {
    refuseUnknownDevice(resp);
    return;
  }
  DeviceInfo info;
  if (!a->getInfo(&info)) {
    refuseUnknownDevice(resp);
    return;
  }
  char* p = resp->payload;
  const char* end = resp->payload + sizeof(resp->payload);
  *p = 0;
  if (!jsonDeviceFull(p, end, info)) return;
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opDeviceStatus(const Mesh485Request& req, Mesh485Response* resp) {
  DeviceAdapter* a = resolveDevice(req, true);
  if (!a) {
    refuseUnknownDevice(resp);
    return;
  }
  DeviceInfo info;
  if (!a->getInfo(&info)) {
    refuseUnknownDevice(resp);
    return;
  }
  snprintf(resp->payload, sizeof(resp->payload),
           "{\"id\":\"%s\",\"online\":%s,\"state\":\"%s\"}", info.id,
           info.online ? "true" : "false", info.online ? "OK" : "OFFLINE");
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opPointList(const Mesh485Request& req, Mesh485Response* resp) {
  char* p = resp->payload;
  const char* end = resp->payload + sizeof(resp->payload);
  if (!jsonAppend(p, end, "{\"points\":[")) return;
  bool first = true;
  const bool filter = req.device_id[0] != 0;
  if (filter && !devices_.byId(req.device_id)) {
    refuseUnknownDevice(resp);
    return;
  }
  DevicePoint pts[DeviceRegistry::kMaxPoints];
  for (size_t i = 0; i < devices_.count(); ++i) {
    DeviceAdapter* a = devices_.byIndex(i);
    if (!a) continue;
    if (filter && strcmp(a->id(), req.device_id) != 0) continue;
    const size_t n = a->getPoints(pts, DeviceRegistry::kMaxPoints);
    for (size_t k = 0; k < n; ++k) {
      if (!jsonPoint(p, end, pts[k], !first)) return;
      first = false;
    }
  }
  if (!jsonAppend(p, end, "]}")) return;
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::opPointStatus(const Mesh485Request& req, Mesh485Response* resp) {
  if (!req.device_id[0] || !req.point_id[0]) {
    refuseUnknownDevice(resp);
    return;
  }
  DeviceAdapter* a = devices_.byId(req.device_id);
  if (!a) {
    refuseUnknownDevice(resp);
    return;
  }
  DevicePoint pts[DeviceRegistry::kMaxPoints];
  const size_t n = a->getPoints(pts, DeviceRegistry::kMaxPoints);
  for (size_t i = 0; i < n; ++i) {
    if (strcmp(pts[i].point_id, req.point_id) == 0) {
      char* p = resp->payload;
      const char* end = resp->payload + sizeof(resp->payload);
      *p = 0;
      if (!jsonPoint(p, end, pts[i], false)) return;
      setError("");
      copyZ(resp->result, sizeof(resp->result), "OK");
      return;
    }
  }
  setError("UNKNOWN_CMD");
  copyZ(resp->result, sizeof(resp->result), "ERR");
  copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
  copyZ(resp->payload, sizeof(resp->payload), "{\"error\":\"unknown point_id\"}");
}

void Mesh485Dispatcher::opEventList(const Mesh485Request& req, Mesh485Response* resp) {
  char* p = resp->payload;
  const char* end = resp->payload + sizeof(resp->payload);
  if (!jsonAppend(p, end, "{\"events\":[")) return;
  bool first = true;
  const bool filter = req.device_id[0] != 0;
  if (filter && !devices_.byId(req.device_id)) {
    refuseUnknownDevice(resp);
    return;
  }
  bool include = false;
  for (int i = 0; i < events_.count(); ++i) {
    EventRecord ev;
    if (!events_.getAt((size_t)i, &ev)) break;
    if (filter && strcmp(ev.device_id, req.device_id) != 0) continue;
    if (!sinceMatch(ev, req.since, &include)) continue;
    char obj[480];
    if (!event_record_json(ev, obj, sizeof(obj))) break;
    const size_t slen = strlen(obj);
    const size_t extra = first ? 0 : 1;
    if ((size_t)(end - p) < slen + extra + 3) break;
    if (!first && !jsonAppend(p, end, ",")) break;
    if (!jsonAppend(p, end, obj)) break;
    first = false;
  }
  if (!jsonAppend(p, end, "]}")) return;
  setError("");
  copyZ(resp->result, sizeof(resp->result), "OK");
}

void Mesh485Dispatcher::handle(const Mesh485Request& req, const Mesh485Env& env,
                               Mesh485Response* resp) {
  if (!resp) return;
  memset(resp, 0, sizeof(*resp));
  fillMeta(req, env, resp);
  copyZ(resp->payload, sizeof(resp->payload), "{}");

  devices_.bindPort(env.port);
  devices_.setTxBlocked(rdm_mode_ || bridge_.isOpen());
  devices_.pollAll();
  harvestEvents();
  bridge_.tick(env.uptime_s);

  if (busy_) {
    if (env.port) env.port->setDirection(false);
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "BUSY");
    return;
  }

  if (replay_.isReplay(req.id, req.optr, req.ts, env.uptime_s, env.unix_now)) {
    setError("REPLAY");
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "REPLAY");
    return;
  }
  replay_.remember(req.id, req.optr, req.ts, env.uptime_s);

  busy_ = true;
  uint8_t presented[MESH485_PUBKEY_LEN] = {};
  bool have_presented = false;
  if (req.pubkey[0]) {
    have_presented = mesh485_parse_pubkey_hex(req.pubkey, presented);
  } else if (env.sender_pub_set) {
    memcpy(presented, env.sender_pub, MESH485_PUBKEY_LEN);
    have_presented = true;
  }
  Mesh485Role authed;
  if (!auth_.resolve(req.optr, presented, have_presented, &authed)) {
    refuseBadAuth(resp);
  } else {
    caller_ = authed;
    if (strcmp(req.op, "AUTH_SET") == 0) {
    opAuthSet(req, resp);
  } else if (strcmp(req.op, "NODE_STATUS") == 0) {
    opNodeStatus(env, resp);
  } else if (strcmp(req.op, "CONFIG_GET") == 0) {
    opConfigGet(env, resp);
  } else if (strcmp(req.op, "CONFIG_SET") == 0) {
    opConfigSet(req, env, resp);
  } else if (strcmp(req.op, "DIAGNOSTICS") == 0) {
    opDiagnostics(env, resp);
  } else if (strcmp(req.op, "RS485_TRANSACTION") == 0) {
    opRs485Transaction(req, env, resp);
  } else if (strcmp(req.op, "BRIDGE_OPEN") == 0) {
    opBridgeOpen(req, env, resp);
  } else if (strcmp(req.op, "BRIDGE_CLOSE") == 0) {
    opBridgeClose(env, resp);
  } else if (strcmp(req.op, "BRIDGE_POLL") == 0 || strcmp(req.op, "BRIDGE_DRAIN") == 0) {
    opBridgePoll(env, resp);
  } else if (strcmp(req.op, "DEVICE_DISCOVER") == 0) {
    opDeviceDiscover(resp);
  } else if (strcmp(req.op, "DEVICE_INFO") == 0) {
    opDeviceInfo(req, resp);
  } else if (strcmp(req.op, "DEVICE_STATUS") == 0) {
    opDeviceStatus(req, resp);
  } else if (strcmp(req.op, "POINT_LIST") == 0) {
    opPointList(req, resp);
  } else if (strcmp(req.op, "POINT_STATUS") == 0) {
    opPointStatus(req, resp);
  } else if (strcmp(req.op, "EVENT_LIST") == 0) {
    opEventList(req, resp);
  } else if (strcmp(req.op, "EVENT_SUBSCRIBE") == 0) {
    opEventSubscribe(req, resp);
  } else if (strcmp(req.op, "COMMAND_EXECUTE") == 0) {
    opCommandExecute(req, env, resp);
#ifdef MESH485_DEBUG
  } else if (strcmp(req.op, "DEBUG_PCN") == 0) {
    const bool closed = strcmp(req.stub, "OPEN") != 0;
    devices_.pcn().debugSetClosed(closed);
    snprintf(resp->payload, sizeof(resp->payload),
             "{\"closed\":%s,\"gpio_low\":%s}", closed ? "true" : "false",
             devices_.pcn().gpioClosed() ? "true" : "false");
    setError("");
    copyZ(resp->result, sizeof(resp->result), "OK");
#endif
  } else if (startsWith(req.op, "ACCESS_")) {
    opAccess(req, env, resp);
  } else {
    copyZ(resp->result, sizeof(resp->result), "ERR");
    copyZ(resp->error, sizeof(resp->error), "UNKNOWN_CMD");
  }
  }
  busy_ = false;
}

size_t Mesh485Dispatcher::handleLine(const char* json, const Mesh485Env& env, char* out,
                                     size_t out_max) {
  static Mesh485Request req;
  static Mesh485Response resp;
  memset(&req, 0, sizeof(req));
  memset(&resp, 0, sizeof(resp));
  copyZ(resp.node, sizeof(resp.node), env.node_id ? env.node_id : MESH485_NODE_ID);
  copyZ(resp.result, sizeof(resp.result), "ERR");
  copyZ(resp.payload, sizeof(resp.payload), "{}");
  if (env.unix_now > 0) resp.ts = env.unix_now;
  else resp.ts = (int64_t)env.uptime_s;

  if (!mesh485_parse_request(json, &req)) {
    copyZ(resp.id, sizeof(resp.id), "0");
    copyZ(resp.op, sizeof(resp.op), "UNKNOWN");
    copyZ(resp.error, sizeof(resp.error), "UNKNOWN_CMD");
    return mesh485_write_response(out, out_max, resp);
  }
  handle(req, env, &resp);
  return mesh485_write_response(out, out_max, resp);
}

void Mesh485Dispatcher::loadRole() {
#ifndef MESH485_NATIVE
  role_source_ = MESH485_ROLE_SOURCE != 0;
  if (SPIFFS.begin(true)) {
    File f = SPIFFS.open("/m485_role.bin", "r");
    if (f) {
      char magic[4];
      uint8_t v = 0;
      if (f.read((uint8_t*)magic, 4) == 4 && memcmp(magic, "ROL1", 4) == 0 && f.read(&v, 1) == 1) {
        role_source_ = v != 0;
      }
      f.close();
    }
  }
  Serial.printf("PCN role %s\n", role_source_ ? "source" : "receiver");
#else
  role_source_ = MESH485_ROLE_SOURCE != 0;
#endif
}

void Mesh485Dispatcher::saveRole() const {
#ifndef MESH485_NATIVE
  if (!SPIFFS.begin(true)) return;
  File f = SPIFFS.open("/m485_role.bin", "w");
  if (!f) return;
  const uint8_t v = role_source_ ? 1 : 0;
  f.write((const uint8_t*)"ROL1", 4);
  f.write(&v, 1);
  f.close();
  Serial.printf("PCN role %s\n", role_source_ ? "source" : "receiver");
#endif
}

void Mesh485Dispatcher::setRoleSource(bool on) {
  role_source_ = on;
  if (!on) {
    beacon_next_ms_ = 0;
    // A source overlay must not stay on the receiver as a raw node id.
    pcn_shown_[0] = 0;
    pcn_from_[0] = 0;
  }
  saveRole();
}

static const char* placeOf(const char* node) {
  if (!node || !node[0]) return "";
  if (strcmp(node, "EXT-001") == 0) return "Ульянова 1";
  if (strcmp(node, "EXT-003") == 0) return "Мира 2";
  return node;
}

static void clockMmSs(uint32_t ms, unsigned* mm, unsigned* ss) {
  const uint32_t s = ms / 1000u;
  *mm = (unsigned)((s / 60u) % 60u);
  *ss = (unsigned)(s % 60u);
}

void Mesh485Dispatcher::pushHist(const char* name, uint32_t start_ms) {
  if (hist_n_ < kHistSlots) {
    copyZ(hist_[hist_n_].name, sizeof(hist_[hist_n_].name), name ? name : "");
    hist_[hist_n_].start_ms = start_ms;
    hist_n_++;
    return;
  }
  memmove(&hist_[0], &hist_[1], sizeof(hist_[0]) * (kHistSlots - 1));
  copyZ(hist_[kHistSlots - 1].name, sizeof(hist_[0].name), name ? name : "");
  hist_[kHistSlots - 1].start_ms = start_ms;
}

void Mesh485Dispatcher::loadChannel() {
#ifndef MESH485_NATIVE
  channel_ = 1;
  if (SPIFFS.begin(true)) {
    File f = SPIFFS.open("/m485_ch.bin", "r");
    if (f) {
      char magic[4];
      uint8_t v = 0;
      if (f.read((uint8_t*)magic, 4) == 4 && memcmp(magic, "CHN1", 4) == 0 && f.read(&v, 1) == 1 &&
          v >= 1 && v <= kPcnChannelMax) {
        channel_ = v;
      }
      f.close();
    }
  }
  Serial.printf("PCN ch %u\n", (unsigned)channel_);
#else
  channel_ = 1;
#endif
}

void Mesh485Dispatcher::saveChannel() const {
#ifndef MESH485_NATIVE
  if (!SPIFFS.begin(true)) return;
  File f = SPIFFS.open("/m485_ch.bin", "w");
  if (!f) return;
  f.write((const uint8_t*)"CHN1", 4);
  f.write(&channel_, 1);
  f.close();
  Serial.printf("PCN ch %u\n", (unsigned)channel_);
#endif
}

void Mesh485Dispatcher::bumpChannel() {
  channel_ = (uint8_t)(channel_ >= kPcnChannelMax ? 1 : channel_ + 1);
  saveChannel();
}

void Mesh485Dispatcher::setAlarmName(const char* name) {
  copyZ(alarm_name_, sizeof(alarm_name_), name && name[0] ? name : "");
}

bool Mesh485Dispatcher::noteHold(const char* node, uint32_t now_ms, const char* label) {
  if (!node || !node[0]) return false;
  const char* shown = (label && label[0]) ? label : placeOf(node);
  int free_i = -1;
  int oldest = -1;
  for (int i = 0; i < kHoldSlots; ++i) {
    if (holds_[i].used && strcmp(holds_[i].node, node) == 0) {
      if (shown[0]) copyZ(holds_[i].label, sizeof(holds_[i].label), shown);
      const bool live = (uint32_t)(now_ms - holds_[i].last_ms) < kPcnHoldMs;
      holds_[i].last_ms = now_ms;
      if (live) return false;
      holds_[i].start_ms = now_ms;
      pushHist(holds_[i].label, now_ms);
      return true;
    }
    if (!holds_[i].used && free_i < 0) free_i = i;
    if (holds_[i].used &&
        (oldest < 0 || (int32_t)(holds_[i].last_ms - holds_[oldest].last_ms) < 0)) {
      oldest = i;
    }
  }
  int slot = free_i >= 0 ? free_i : oldest;
  if (slot < 0) return false;
  copyZ(holds_[slot].node, sizeof(holds_[slot].node), node);
  copyZ(holds_[slot].label, sizeof(holds_[slot].label), shown);
  holds_[slot].last_ms = now_ms;
  holds_[slot].start_ms = now_ms;
  holds_[slot].used = 1;
  pushHist(shown, now_ms);
  return true;
}

bool Mesh485Dispatcher::holdLive(uint32_t now_ms) const {
  for (int i = 0; i < kHoldSlots; ++i) {
    if (!holds_[i].used) continue;
    if ((uint32_t)(now_ms - holds_[i].last_ms) < kPcnHoldMs) return true;
  }
  return false;
}

const char* Mesh485Dispatcher::liveHoldNode(uint32_t now_ms) const {
  const char* any = nullptr;
  for (int i = 0; i < kHoldSlots; ++i) {
    if (!holds_[i].used) continue;
    any = holds_[i].node;
    if ((uint32_t)(now_ms - holds_[i].last_ms) < kPcnHoldMs) return holds_[i].node;
  }
  return any;
}

const char* Mesh485Dispatcher::pcnShownState() const {
  if (!role_source_) return holdLive(mesh485_now_ms()) ? "ALARM" : "NORMAL";
  return pcn_shown_;
}

const char* Mesh485Dispatcher::pcnFromNode() const {
  if (!role_source_) {
    const char* live = liveHoldNode(mesh485_now_ms());
    if (live && live[0]) return live;
  }
  return pcn_from_[0] ? pcn_from_ : node_id_;
}

static void formatHoldLine(char* dst, size_t n, const char* label, uint32_t start_ms, uint32_t last_ms,
                           uint32_t now) {
  if (!dst || !n) return;
  unsigned mm = 0;
  unsigned ss = 0;
  clockMmSs(start_ms, &mm, &ss);
  uint32_t left = 0;
  const uint32_t age = (uint32_t)(now - last_ms);
  if (age < kPcnHoldMs) left = (kPcnHoldMs - age) / 1000u;
  snprintf(dst, n, "%s %02u:%02u %uс", label && label[0] ? label : "", mm, ss, (unsigned)left);
}

void Mesh485Dispatcher::fillAlarm(PcnScreen* screen) const {
  if (!screen) return;
  *screen = PcnScreen();
  screen->source = role_source_;
  screen->channel = channel_;
  const uint32_t now = mesh485_now_ms();
  if (role_source_) {
    screen->active = devices_.pcn().alarm() ? 1 : 0;
    if (screen->active) {
      copyZ(screen->line_a, sizeof(screen->line_a),
            alarm_name_[0] ? alarm_name_ : node_id_);
    }
  } else {
    const PcnRxHold* picked[2] = {nullptr, nullptr};
    for (int i = 0; i < kHoldSlots; ++i) {
      if (!holds_[i].used) continue;
      if ((uint32_t)(now - holds_[i].last_ms) >= kPcnHoldMs) continue;
      screen->active++;
      const bool uly = strcmp(holds_[i].node, "EXT-001") == 0;
      const bool mira = strcmp(holds_[i].node, "EXT-003") == 0;
      if (uly) picked[0] = &holds_[i];
      else if (mira) picked[1] = &holds_[i];
      else if (!picked[0]) picked[0] = &holds_[i];
      else if (!picked[1]) picked[1] = &holds_[i];
    }
    if (picked[0])
      formatHoldLine(screen->line_a, sizeof(screen->line_a), picked[0]->label, picked[0]->start_ms,
                     picked[0]->last_ms, now);
    if (picked[1])
      formatHoldLine(screen->line_b, sizeof(screen->line_b), picked[1]->label, picked[1]->start_ms,
                     picked[1]->last_ms, now);
  }
  const int n = hist_n_ < 3 ? hist_n_ : 3;
  screen->hist_n = (uint8_t)n;
  for (int i = 0; i < n; ++i) {
    const PcnHist& h = hist_[hist_n_ - 1 - i];
    unsigned mm = 0;
    unsigned ss = 0;
    clockMmSs(h.start_ms, &mm, &ss);
    snprintf(screen->hist[i], sizeof(screen->hist[i]), "%s %02u:%02u", h.name, mm, ss);
  }
}
