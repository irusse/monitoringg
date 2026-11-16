#include "mesh485/bridge.h"

#include "mesh485/base64.h"

#include <stdio.h>
#include <string.h>

static void copyZ(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

Mesh485BridgeLevel Mesh485Bridge::parseLevel(const char* s) {
  if (!s || !s[0] || strcmp(s, "READ_ONLY") == 0) return Mesh485BridgeLevel::ReadOnly;
  if (strcmp(s, "CONTROL") == 0) return Mesh485BridgeLevel::Control;
  if (strcmp(s, "RAW_BRIDGE") == 0) return Mesh485BridgeLevel::Raw;
  return Mesh485BridgeLevel::Unknown;
}

const char* Mesh485Bridge::levelName(Mesh485BridgeLevel lv) {
  switch (lv) {
    case Mesh485BridgeLevel::ReadOnly:
      return "READ_ONLY";
    case Mesh485BridgeLevel::Control:
      return "CONTROL";
    case Mesh485BridgeLevel::Raw:
      return "RAW_BRIDGE";
    case Mesh485BridgeLevel::Off:
      return "off";
    default:
      return "UNKNOWN";
  }
}

uint32_t Mesh485Bridge::idleRemaining(uint32_t now_s) const {
  if (!open_) return 0;
  const uint32_t elapsed = (now_s >= last_s_) ? (now_s - last_s_) : 0;
  if (elapsed >= idle_s_) return 0;
  return idle_s_ - elapsed;
}

void Mesh485Bridge::failSafeRx() {
  if (port_) port_->setDirection(false);
}

void Mesh485Bridge::clearRing() {
  head_ = 0;
  count_ = 0;
  dropped_ = 0;
  memset(ring_, 0, sizeof(ring_));
}

bool Mesh485Bridge::open(Mesh485BridgeLevel lv, uint32_t idle_s, const char* optr, int64_t ts,
                         uint32_t now_s, Mesh485Port* port) {
  if (open_) return false;
  if (lv == Mesh485BridgeLevel::Unknown || lv == Mesh485BridgeLevel::Off) return false;

  port_ = port;
  level_ = lv;
  idle_s_ = idle_s;
  if (idle_s_ == 0) idle_s_ = MESH485_BRIDGE_IDLE_DEFAULT_S;
  if (idle_s_ > MESH485_BRIDGE_IDLE_MAX_S) idle_s_ = MESH485_BRIDGE_IDLE_MAX_S;
  last_s_ = now_s;
  open_ = true;
  clearRing();
  failSafeRx();

  last_.used = true;
  copyZ(last_.optr, sizeof(last_.optr), optr ? optr : "");
  last_.ts = ts;
  copyZ(last_.level, sizeof(last_.level), levelName(lv));
  return true;
}

void Mesh485Bridge::close() {
  failSafeRx();
  open_ = false;
  level_ = Mesh485BridgeLevel::Off;
  clearRing();
  port_ = nullptr;
}

void Mesh485Bridge::touch(uint32_t now_s) {
  if (open_) last_s_ = now_s;
}

void Mesh485Bridge::tick(uint32_t now_s) {
  if (!open_) return;
  if (idleRemaining(now_s) == 0 && now_s >= last_s_ && (now_s - last_s_) >= idle_s_) {
    close();
  }
}

void Mesh485Bridge::capture(const uint8_t* data, size_t len, int64_t ts) {
  if (!open_ || !data || len == 0) return;
  if (len > MESH485_BRIDGE_SLOT_MAX) len = MESH485_BRIDGE_SLOT_MAX;

  if (count_ >= MESH485_BRIDGE_RING_SLOTS) {
    head_ = (uint8_t)((head_ + 1) % MESH485_BRIDGE_RING_SLOTS);
    --count_;
    ++dropped_;
  }
  const uint8_t i = (uint8_t)((head_ + count_) % MESH485_BRIDGE_RING_SLOTS);
  memcpy(ring_[i].data, data, len);
  ring_[i].len = (uint16_t)len;
  ring_[i].ts = ts;
  ++count_;
}

void Mesh485Bridge::drainPayload(char* out, size_t out_max) {
  if (!out || out_max < 48) return;
  const char* lv = open_ ? levelName() : "off";
  const uint32_t dropped = dropped_;
  dropped_ = 0;

  int n = snprintf(out, out_max,
                   "{\"session\":%s,\"level\":\"%s\",\"frames\":[",
                   open_ ? "true" : "false", lv);
  if (n < 0 || (size_t)n >= out_max) {
    copyZ(out, out_max, "{\"session\":false,\"level\":\"off\",\"frames\":[],\"dropped\":0}");
    return;
  }

  bool first = true;
  while (count_ > 0) {
    const Slot& sl = ring_[head_];
    char b64[MESH485_B64_MAX];
    b64[0] = 0;
    if (sl.len) mesh485_b64_encode(sl.data, sl.len, b64, sizeof(b64));

    char one[MESH485_B64_MAX + 48];
    snprintf(one, sizeof(one), "%s{\"ts\":%lld,\"frame\":\"%s\"}", first ? "" : ",",
             (long long)sl.ts, b64);

    const size_t used = strlen(out);
    const size_t need = strlen(one) + 24;  // ,"dropped":N}
    if (used + need >= out_max) break;

    memcpy(out + used, one, strlen(one) + 1);
    first = false;
    head_ = (uint8_t)((head_ + 1) % MESH485_BRIDGE_RING_SLOTS);
    --count_;
  }

  char tail[40];
  snprintf(tail, sizeof(tail), "],\"dropped\":%lu}", (unsigned long)dropped);
  const size_t used = strlen(out);
  if (used + strlen(tail) + 1 > out_max) {
    copyZ(out, out_max, "{\"session\":true,\"level\":\"READ_ONLY\",\"frames\":[],\"dropped\":0}");
    return;
  }
  memcpy(out + used, tail, strlen(tail) + 1);
}
