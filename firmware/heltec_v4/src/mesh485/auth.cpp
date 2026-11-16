#include "mesh485/auth.h"

#include <stdio.h>
#include <string.h>

#ifndef MESH485_NATIVE
#include <Arduino.h>
#include <SPIFFS.h>
static const char kPath[] = "/m485_auth.bin";
#endif

#ifdef MESH485_NATIVE
static bool g_auth_fake_fs_on = false;
static uint8_t g_auth_fake_fs[sizeof(Mesh485AuthHeader) +
                              MESH485_AUTH_CAP * sizeof(Mesh485AuthRecordV2)];
static size_t g_auth_fake_fs_len = 0;
#endif

void mesh485_auth_use_fake_fs(bool on) {
#ifdef MESH485_NATIVE
  g_auth_fake_fs_on = on;
#else
  (void)on;
#endif
}

void mesh485_auth_reset_fake_fs() {
#ifdef MESH485_NATIVE
  g_auth_fake_fs_len = 0;
  memset(g_auth_fake_fs, 0, sizeof(g_auth_fake_fs));
#endif
}

void mesh485_auth_poke_fake_fs(const uint8_t* data, size_t len) {
#ifdef MESH485_NATIVE
  if (!data || len > sizeof(g_auth_fake_fs)) {
    g_auth_fake_fs_len = 0;
    return;
  }
  memcpy(g_auth_fake_fs, data, len);
  if (len < sizeof(g_auth_fake_fs)) memset(g_auth_fake_fs + len, 0, sizeof(g_auth_fake_fs) - len);
  g_auth_fake_fs_len = len;
#else
  (void)data;
  (void)len;
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

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool mesh485_parse_pubkey_hex(const char* hex, uint8_t out[MESH485_PUBKEY_LEN]) {
  if (!hex || !out) return false;
  size_t n = 0;
  while (hex[n]) ++n;
  if (n != MESH485_PUBKEY_LEN * 2) return false;
  for (size_t i = 0; i < MESH485_PUBKEY_LEN; ++i) {
    const int hi = hexNibble(hex[i * 2]);
    const int lo = hexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

bool mesh485_pubkey_empty(const uint8_t p[MESH485_PUBKEY_LEN]) {
  if (!p) return true;
  for (size_t i = 0; i < MESH485_PUBKEY_LEN; ++i) {
    if (p[i]) return false;
  }
  return true;
}

static void authLogFactory(const char* why) {
#ifdef MESH485_NATIVE
  printf("mesh485 auth: %s — factory viewer/operator/admin\n", why);
#else
  Serial.printf("mesh485 auth: %s — factory viewer/operator/admin\n", why);
#endif
}

Mesh485Auth::Mesh485Auth() { seedFactory(); }

void Mesh485Auth::resetRam() {
  memset(rows_, 0, sizeof(rows_));
  n_ = 0;
}

void Mesh485Auth::seedFactory() {
  resetRam();
  set("viewer", Mesh485Role::Viewer);
  set("operator", Mesh485Role::Operator);
  set("admin", Mesh485Role::Admin);
}

bool Mesh485Auth::rowsHaveAdmin() const { return adminCount() >= 1; }

void Mesh485Auth::begin() {
  resetRam();
  if (load() && rowsHaveAdmin()) return;
  if (n_ != 0 && !rowsHaveAdmin()) {
    authLogFactory("file has no ADMIN");
  } else if (n_ == 0) {
    authLogFactory("no file or bad blob");
  }
  seedFactory();
}

int Mesh485Auth::find(const char* name) const {
  if (!name || !name[0]) return -1;
  for (uint8_t i = 0; i < n_; ++i) {
    if (mesh485_ieq(rows_[i].name, name)) return (int)i;
  }
  return -1;
}

bool Mesh485Auth::lookup(const char* name, Mesh485Role* out) const {
  const int i = find(name);
  if (i < 0) return false;
  if (out) *out = rows_[i].role;
  return true;
}

bool Mesh485Auth::hasPubkey(const char* name) const {
  const int i = find(name);
  return i >= 0 && !mesh485_pubkey_empty(rows_[i].pubkey);
}

bool Mesh485Auth::pubkeyOf(const char* name, uint8_t out[MESH485_PUBKEY_LEN]) const {
  const int i = find(name);
  if (i < 0) return false;
  if (out) memcpy(out, rows_[i].pubkey, MESH485_PUBKEY_LEN);
  return true;
}

bool Mesh485Auth::resolve(const char* name, const uint8_t* presented, bool have_presented,
                          Mesh485Role* out) const {
  const int i = find(name);
  if (i < 0) return false;
  const Mesh485Role stored = rows_[i].role;
  const Mesh485Role claimed = mesh485_role(name);
  if ((uint8_t)claimed > (uint8_t)stored) return false;
  if (!mesh485_pubkey_empty(rows_[i].pubkey)) {
    if (!have_presented || !presented) return false;
    if (memcmp(rows_[i].pubkey, presented, MESH485_PUBKEY_LEN) != 0) return false;
  }
  if (out) *out = stored;
  return true;
}

Mesh485Role Mesh485Auth::roleOf(const char* name) const {
  Mesh485Role r = Mesh485Role::Viewer;
  if (!resolve(name, &r)) return Mesh485Role::Viewer;
  return r;
}

bool Mesh485Auth::set(const char* name, Mesh485Role role) {
  if (!name || !name[0]) return false;
  const int i = find(name);
  if (i >= 0) {
    rows_[i].role = role;
    return true;
  }
  if (n_ >= MESH485_AUTH_CAP) return false;
  copyZ(rows_[n_].name, sizeof(rows_[n_].name), name);
  rows_[n_].role = role;
  memset(rows_[n_].pubkey, 0, sizeof(rows_[n_].pubkey));
  ++n_;
  return true;
}

bool Mesh485Auth::setPubkey(const char* name, const uint8_t pub[MESH485_PUBKEY_LEN]) {
  const int i = find(name);
  if (i < 0 || !pub) return false;
  memcpy(rows_[i].pubkey, pub, MESH485_PUBKEY_LEN);
  return true;
}

bool Mesh485Auth::clearPubkey(const char* name) {
  const int i = find(name);
  if (i < 0) return false;
  memset(rows_[i].pubkey, 0, sizeof(rows_[i].pubkey));
  return true;
}

bool Mesh485Auth::remove(const char* name) {
  const int i = find(name);
  if (i < 0) return false;
  const uint8_t last = (uint8_t)(n_ - 1);
  if ((uint8_t)i != last) rows_[i] = rows_[last];
  memset(&rows_[last], 0, sizeof(rows_[last]));
  --n_;
  return true;
}

uint8_t Mesh485Auth::adminCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < n_; ++i) {
    if (rows_[i].role == Mesh485Role::Admin) ++n;
  }
  return n;
}

const Mesh485AuthEntry* Mesh485Auth::at(uint8_t i) const {
  return i < n_ ? &rows_[i] : nullptr;
}

bool Mesh485Auth::persist() const {
  if (!rowsHaveAdmin()) return false;
  Mesh485AuthHeader h;
  memset(&h, 0, sizeof(h));
  h.magic = kMagic;
  h.version = kVersion;
  h.count = n_;
  h.cap = (uint16_t)MESH485_AUTH_CAP;
  Mesh485AuthRecordV2 recs[MESH485_AUTH_CAP];
  memset(recs, 0, sizeof(recs));
  for (uint8_t i = 0; i < n_; ++i) {
    copyZ(recs[i].name, sizeof(recs[i].name), rows_[i].name);
    recs[i].role = (uint8_t)rows_[i].role;
    memcpy(recs[i].pubkey, rows_[i].pubkey, MESH485_PUBKEY_LEN);
  }
#ifdef MESH485_NATIVE
  if (!g_auth_fake_fs_on) return true;
  memcpy(g_auth_fake_fs, &h, sizeof(h));
  memcpy(g_auth_fake_fs + sizeof(h), recs, sizeof(recs));
  g_auth_fake_fs_len = sizeof(h) + sizeof(recs);
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open(kPath, "w");
  if (!f) return false;
  const size_t wh = f.write((const uint8_t*)&h, sizeof(h));
  const size_t wr = f.write((const uint8_t*)recs, sizeof(recs));
  f.close();
  return wh == sizeof(h) && wr == sizeof(recs);
#endif
}

bool Mesh485Auth::load() {
  Mesh485AuthHeader h;
  Mesh485AuthRecord recs_v1[MESH485_AUTH_CAP];
  Mesh485AuthRecordV2 recs_v2[MESH485_AUTH_CAP];
  uint16_t ver = 0;
#ifdef MESH485_NATIVE
  if (!g_auth_fake_fs_on || g_auth_fake_fs_len < sizeof(Mesh485AuthHeader)) return false;
  memcpy(&h, g_auth_fake_fs, sizeof(h));
  if (h.magic != kMagic || h.cap != MESH485_AUTH_CAP || h.count > MESH485_AUTH_CAP) return false;
  if (h.version == kVersionV1) {
    if (g_auth_fake_fs_len < sizeof(h) + sizeof(recs_v1)) return false;
    memcpy(recs_v1, g_auth_fake_fs + sizeof(h), sizeof(recs_v1));
    ver = kVersionV1;
  } else if (h.version == kVersion) {
    if (g_auth_fake_fs_len < sizeof(h) + sizeof(recs_v2)) return false;
    memcpy(recs_v2, g_auth_fake_fs + sizeof(h), sizeof(recs_v2));
    ver = kVersion;
  } else {
    return false;
  }
#else
  if (!SPIFFS.begin(true)) return false;
  if (!SPIFFS.exists(kPath)) return false;
  File f = SPIFFS.open(kPath, "r");
  if (!f) return false;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) {
    f.close();
    return false;
  }
  if (h.magic != kMagic || h.cap != MESH485_AUTH_CAP || h.count > MESH485_AUTH_CAP) {
    f.close();
    return false;
  }
  if (h.version == kVersionV1) {
    if (f.read((uint8_t*)recs_v1, sizeof(recs_v1)) != (int)sizeof(recs_v1)) {
      f.close();
      return false;
    }
    ver = kVersionV1;
  } else if (h.version == kVersion) {
    if (f.read((uint8_t*)recs_v2, sizeof(recs_v2)) != (int)sizeof(recs_v2)) {
      f.close();
      return false;
    }
    ver = kVersion;
  } else {
    f.close();
    return false;
  }
  f.close();
#endif
  resetRam();
  if (ver == kVersionV1) {
    for (uint16_t i = 0; i < h.count; ++i) {
      if (!recs_v1[i].name[0]) return false;
      if (recs_v1[i].role > (uint8_t)Mesh485Role::Admin) return false;
      if (!set(recs_v1[i].name, (Mesh485Role)recs_v1[i].role)) return false;
    }
    return n_ == h.count;
  }
  for (uint16_t i = 0; i < h.count; ++i) {
    if (!recs_v2[i].name[0]) return false;
    if (recs_v2[i].role > (uint8_t)Mesh485Role::Admin) return false;
    if (!set(recs_v2[i].name, (Mesh485Role)recs_v2[i].role)) return false;
    if (!mesh485_pubkey_empty(recs_v2[i].pubkey)) {
      if (!setPubkey(recs_v2[i].name, recs_v2[i].pubkey)) return false;
    }
  }
  return n_ == h.count;
}
