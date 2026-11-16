#include "access/access_controller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MESH485_NATIVE
#include <Arduino.h>
#include <SPIFFS.h>
static const char kPath[] = "/m485_acl.bin";
#endif

#ifdef MESH485_NATIVE
static bool g_acl_fake_fs_on = false;
static uint8_t g_acl_fake_fs[sizeof(AccessAclHeader) + ACCESS_ACL_CAP * sizeof(AccessAclRecord)];
static size_t g_acl_fake_fs_len = 0;
#endif

void access_acl_use_fake_fs(bool on) {
#ifdef MESH485_NATIVE
  g_acl_fake_fs_on = on;
#else
  (void)on;
#endif
}

void access_acl_reset_fake_fs() {
#ifdef MESH485_NATIVE
  g_acl_fake_fs_len = 0;
  memset(g_acl_fake_fs, 0, sizeof(g_acl_fake_fs));
#endif
}

static void copyZ(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

bool AccessController::canonicalizeUid(const char* in, char* out, size_t max) {
  if (!out || max == 0) return false;
  out[0] = 0;
  if (!in) return false;
  size_t n = 0;
  for (const char* p = in; *p; ++p) {
    char c = *p;
    if (c == ' ' || c == ':' || c == '-' || c == '\t') continue;
    if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) continue;
    if (n + 1 >= max) return false;
    out[n++] = c;
  }
  out[n] = 0;
  return n > 0;
}

void AccessController::resetRam() {
  if (!ensureBuf()) {
    count_ = 0;
    last_uid_[0] = 0;
    return;
  }
  memset(recs_, 0, sizeof(AccessAclRecord) * kCap);
  count_ = 0;
  last_uid_[0] = 0;
}

bool AccessController::ensureBuf() {
  if (recs_) return true;
  recs_ = (AccessAclRecord*)malloc(sizeof(AccessAclRecord) * kCap);
  if (recs_) memset(recs_, 0, sizeof(AccessAclRecord) * kCap);
  return recs_ != nullptr;
}

void AccessController::seedTestUid() {
#if defined(MESH485_ALLOW_TEST_UID) && MESH485_ALLOW_TEST_UID
  if (count_ == 0) allowUid(MESH485_TEST_UID);
#else
  (void)0;
#endif
}

void AccessController::begin() {
  if (!ensureBuf()) {
    ready_ = false;
    return;
  }
  resetRam();
  ready_ = true;
  if (!load()) {
    seedTestUid();
    if (count_ > 0) persist();
    return;
  }
  seedTestUid();
}

int AccessController::findUid(const char* canon) const {
  if (!canon || !canon[0]) return -1;
  for (uint16_t i = 0; i < count_; ++i) {
    if (strcmp(recs_[i].uid, canon) == 0) return (int)i;
  }
  return -1;
}

uint16_t AccessController::enabledCount() const {
  uint16_t n = 0;
  for (uint16_t i = 0; i < count_; ++i) {
    if (recs_[i].enabled) ++n;
  }
  return n;
}

const AccessAclRecord* AccessController::recordAt(uint16_t i) const {
  if (!recs_ || i >= count_) return nullptr;
  return &recs_[i];
}

AccessDecision AccessController::checkUid(const char* uid) {
  if (!ready_) begin();
  AccessDecision d;
  memset(&d, 0, sizeof(d));
  char canon[24] = {};
  if (!canonicalizeUid(uid, canon, sizeof(canon))) {
    d.result = AccessResult::Deny;
    copyZ(d.reason, sizeof(d.reason), "EMPTY");
    return d;
  }
  copyZ(last_uid_, sizeof(last_uid_), canon);
  const int idx = findUid(canon);
  if (idx < 0 || !recs_[idx].enabled) {
    d.result = AccessResult::Deny;
    copyZ(d.reason, sizeof(d.reason), idx < 0 ? "UNKNOWN" : "DISABLED");
    return d;
  }
  d.result = AccessResult::Allow;
  copyZ(d.employee_id, sizeof(d.employee_id), recs_[idx].employee_id);
  copyZ(d.name, sizeof(d.name), recs_[idx].name[0] ? recs_[idx].name : recs_[idx].uid);
  return d;
}

bool AccessController::allowUid(const char* hex) { return addCard(hex, "", "", true); }

bool AccessController::addCard(const char* uid, const char* employee_id, const char* name,
                               bool enabled) {
  if (!ready_) begin();
  char canon[24] = {};
  if (!canonicalizeUid(uid, canon, sizeof(canon))) return false;
  const int idx = findUid(canon);
  if (idx >= 0) {
    recs_[idx].enabled = enabled ? 1 : 0;
    if (employee_id) copyZ(recs_[idx].employee_id, sizeof(recs_[idx].employee_id), employee_id);
    if (name) copyZ(recs_[idx].name, sizeof(recs_[idx].name), name);
    return persist();
  }
  if (count_ >= kCap) return false;
  AccessAclRecord& r = recs_[count_];
  memset(&r, 0, sizeof(r));
  copyZ(r.uid, sizeof(r.uid), canon);
  r.enabled = enabled ? 1 : 0;
  copyZ(r.employee_id, sizeof(r.employee_id), employee_id);
  copyZ(r.name, sizeof(r.name), name);
  ++count_;
  return persist();
}

bool AccessController::removeCard(const char* uid) {
  if (!ready_) begin();
  char canon[24] = {};
  if (!canonicalizeUid(uid, canon, sizeof(canon))) return false;
  const int idx = findUid(canon);
  if (idx < 0) return false;
  if ((uint16_t)idx + 1 < count_) {
    memmove(&recs_[idx], &recs_[idx + 1], (size_t)(count_ - (uint16_t)idx - 1) * sizeof(recs_[0]));
  }
  --count_;
  memset(&recs_[count_], 0, sizeof(recs_[0]));
  return persist();
}

bool AccessController::persist() {
  if (!recs_) return false;
#ifdef MESH485_NATIVE
  if (!g_acl_fake_fs_on) return true;
  AccessAclHeader h;
  memset(&h, 0, sizeof(h));
  h.magic = kMagic;
  h.version = 1;
  h.count = count_;
  h.cap = (uint16_t)kCap;
  memcpy(g_acl_fake_fs, &h, sizeof(h));
  memcpy(g_acl_fake_fs + sizeof(h), recs_, sizeof(AccessAclRecord) * kCap);
  g_acl_fake_fs_len = sizeof(h) + sizeof(AccessAclRecord) * kCap;
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open(kPath, "w");
  if (!f) return false;
  AccessAclHeader h;
  memset(&h, 0, sizeof(h));
  h.magic = kMagic;
  h.version = 1;
  h.count = count_;
  h.cap = (uint16_t)kCap;
  const size_t rec_bytes = sizeof(AccessAclRecord) * kCap;
  const size_t wh = f.write((const uint8_t*)&h, sizeof(h));
  const size_t wr = f.write((const uint8_t*)recs_, rec_bytes);
  f.close();
  return wh == sizeof(h) && wr == rec_bytes;
#endif
}

bool AccessController::load() {
  if (!ensureBuf()) return false;
#ifdef MESH485_NATIVE
  if (!g_acl_fake_fs_on || g_acl_fake_fs_len < sizeof(AccessAclHeader)) return false;
  AccessAclHeader h;
  memcpy(&h, g_acl_fake_fs, sizeof(h));
  if (h.magic != kMagic || h.version != 1 || h.cap != kCap) return false;
  if (h.count > kCap) return false;
  memcpy(recs_, g_acl_fake_fs + sizeof(h), sizeof(AccessAclRecord) * kCap);
  count_ = h.count;
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  if (!SPIFFS.exists(kPath)) return false;
  File f = SPIFFS.open(kPath, "r");
  if (!f) return false;
  AccessAclHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) {
    f.close();
    return false;
  }
  if (h.magic != kMagic || h.version != 1 || h.cap != kCap || h.count > kCap) {
    f.close();
    return false;
  }
  if (f.read((uint8_t*)recs_, sizeof(AccessAclRecord) * kCap) !=
      (int)(sizeof(AccessAclRecord) * kCap)) {
    f.close();
    return false;
  }
  f.close();
  count_ = h.count;
  return true;
#endif
}
