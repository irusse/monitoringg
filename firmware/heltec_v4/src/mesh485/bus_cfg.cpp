#include "mesh485/bus_cfg.h"

#include "device/registry.h"

#include <stdio.h>
#include <string.h>

#ifndef MESH485_NATIVE
#include <Arduino.h>
#include <SPIFFS.h>
static const char kPath[] = "/m485_cfg.bin";
#endif

#ifdef MESH485_NATIVE
static bool g_cfg_fake_fs_on = false;
static uint8_t g_cfg_fake_fs[sizeof(Mesh485CfgBlob)];
static size_t g_cfg_fake_fs_len = 0;
#endif

void mesh485_cfg_use_fake_fs(bool on) {
#ifdef MESH485_NATIVE
  g_cfg_fake_fs_on = on;
#else
  (void)on;
#endif
}

void mesh485_cfg_reset_fake_fs() {
#ifdef MESH485_NATIVE
  g_cfg_fake_fs_len = 0;
  memset(g_cfg_fake_fs, 0, sizeof(g_cfg_fake_fs));
#endif
}

void mesh485_cfg_poke_fake_fs(const uint8_t* data, size_t len) {
#ifdef MESH485_NATIVE
  if (!data || len > sizeof(g_cfg_fake_fs)) {
    g_cfg_fake_fs_len = 0;
    return;
  }
  memcpy(g_cfg_fake_fs, data, len);
  if (len < sizeof(g_cfg_fake_fs)) memset(g_cfg_fake_fs + len, 0, sizeof(g_cfg_fake_fs) - len);
  g_cfg_fake_fs_len = len;
#else
  (void)data;
  (void)len;
#endif
}

size_t mesh485_cfg_fake_fs_len() {
#ifdef MESH485_NATIVE
  return g_cfg_fake_fs_len;
#else
  return 0;
#endif
}

size_t mesh485_cfg_fake_fs_copy(uint8_t* dst, size_t max) {
#ifdef MESH485_NATIVE
  size_t n = g_cfg_fake_fs_len;
  if (n > max) n = max;
  if (dst && n) memcpy(dst, g_cfg_fake_fs, n);
  return g_cfg_fake_fs_len;
#else
  (void)dst;
  (void)max;
  return 0;
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

static void cfgLogRaw(const char* why) {
#ifdef MESH485_NATIVE
  printf("mesh485 cfg: %s — protocol RAW\n", why ? why : "fallback");
#else
  Serial.printf("mesh485 cfg: %s — protocol RAW\n", why ? why : "fallback");
#endif
}

Mesh485BusCfg::Mesh485BusCfg() { resetRam(); }

void Mesh485BusCfg::resetRam() { copyZ(protocol_, sizeof(protocol_), "RAW"); }

void Mesh485BusCfg::begin() {
  resetRam();
  const char* why = nullptr;
  if (load(&why)) return;
  cfgLogRaw(why ? why : "no file or bad blob");
  resetRam();
}

bool Mesh485BusCfg::setAndPersist(const char* proto) {
  if (!DeviceRegistry::busProtocolAllowed(proto)) return false;
  copyZ(protocol_, sizeof(protocol_), proto);
  return persist();
}

bool Mesh485BusCfg::persist() const {
  if (!DeviceRegistry::busProtocolAllowed(protocol_)) return false;
  Mesh485CfgBlob blob;
  memset(&blob, 0, sizeof(blob));
  blob.magic = kMagic;
  blob.version = kVersion;
  copyZ(blob.protocol, sizeof(blob.protocol), protocol_);
#ifdef MESH485_NATIVE
  if (!g_cfg_fake_fs_on) return true;
  memcpy(g_cfg_fake_fs, &blob, sizeof(blob));
  g_cfg_fake_fs_len = sizeof(blob);
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open(kPath, "w");
  if (!f) return false;
  const size_t w = f.write((const uint8_t*)&blob, sizeof(blob));
  f.close();
  return w == sizeof(blob);
#endif
}

bool Mesh485BusCfg::load(const char** why) {
  Mesh485CfgBlob blob;
  memset(&blob, 0, sizeof(blob));
#ifdef MESH485_NATIVE
  if (!g_cfg_fake_fs_on || g_cfg_fake_fs_len == 0) {
    if (why) *why = "no file";
    return false;
  }
  if (g_cfg_fake_fs_len < sizeof(Mesh485CfgBlob)) {
    if (why) *why = "bad blob";
    return false;
  }
  memcpy(&blob, g_cfg_fake_fs, sizeof(blob));
#else
  if (!SPIFFS.begin(true)) {
    if (why) *why = "no file";
    return false;
  }
  if (!SPIFFS.exists(kPath)) {
    if (why) *why = "no file";
    return false;
  }
  File f = SPIFFS.open(kPath, "r");
  if (!f) {
    if (why) *why = "no file";
    return false;
  }
  const int n = f.read((uint8_t*)&blob, sizeof(blob));
  f.close();
  if (n != (int)sizeof(blob)) {
    if (why) *why = "bad blob";
    return false;
  }
#endif
  if (blob.magic != kMagic) {
    if (why) *why = "bad magic";
    return false;
  }
  if (blob.version != kVersion) {
    if (why) *why = "bad blob";
    return false;
  }
  blob.protocol[sizeof(blob.protocol) - 1] = 0;
  if (strcmp(blob.protocol, "AUTO") == 0) {
    if (why) *why = "AUTO in file";
    return false;
  }
  if (!DeviceRegistry::busProtocolAllowed(blob.protocol)) {
    if (why) *why = "unknown protocol";
    return false;
  }
  copyZ(protocol_, sizeof(protocol_), blob.protocol);
  return true;
}
