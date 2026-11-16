#include "mesh485/ble_mux.h"

#include <string.h>

void BleMuxInterface::bind(SerialBLEInterface& inner) {
  inner_ = &inner;
  mode_ = Mode::Idle;
  json_len_ = 0;
}

void BleMuxInterface::setHandler(JsonHandler fn, void* ctx) {
  handler_ = fn;
  handler_ctx_ = ctx;
}

void BleMuxInterface::enable() {
  if (inner_) inner_->enable();
}

void BleMuxInterface::disable() {
  if (inner_) inner_->disable();
}

bool BleMuxInterface::isEnabled() const {
  return inner_ ? inner_->isEnabled() : false;
}

bool BleMuxInterface::isConnected() const {
  return inner_ ? inner_->isConnected() : false;
}

bool BleMuxInterface::isWriteBusy() const {
  return inner_ ? inner_->isWriteBusy() : true;
}

size_t BleMuxInterface::writeFrame(const uint8_t src[], size_t len) {
  return inner_ ? inner_->writeFrame(src, len) : 0;
}

size_t BleMuxInterface::writeJsonLine(const char* line, size_t n) {
  if (!inner_ || !line || n == 0) return 0;
  size_t sent = 0;
  while (sent < n) {
    const size_t chunk = n - sent > MAX_FRAME_SIZE ? MAX_FRAME_SIZE : n - sent;
    if (!inner_->writeFrame((const uint8_t*)line + sent, chunk)) break;
    sent += chunk;
  }
  const uint8_t nl = (uint8_t)'\n';
  inner_->writeFrame(&nl, 1);
  return sent;
}

void BleMuxInterface::finishJson() {
  json_[json_len_ < kJsonMax ? json_len_ : kJsonMax - 1] = 0;
  if (json_len_ > 0 && handler_) {
    static char out[MESH485_RESP_MAX];
    const size_t n = handler_(json_, out, sizeof(out), handler_ctx_);
    if (n) writeJsonLine(out, n);
  }
  mode_ = Mode::Idle;
  json_len_ = 0;
}

void BleMuxInterface::feed(const uint8_t* buf, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    const uint8_t c = buf[i];
    if (c == '\n' || json_len_ >= kJsonMax - 1) {
      if (c != '\n' && c != '\r' && json_len_ < kJsonMax - 1) {
        json_[json_len_++] = (char)c;
      }
      finishJson();
      // Remainder of this NUS write (if any) starts a new frame.
      if (c != '\n' && i + 1 < n && buf[i + 1] == '{') {
        mode_ = Mode::Json;
        json_start_ms_ = millis();
      }
    } else if (c != '\r') {
      json_[json_len_++] = (char)c;
    }
  }
}

size_t BleMuxInterface::checkRecvFrame(uint8_t dest[]) {
  if (!inner_) return 0;

  uint8_t tmp[MAX_FRAME_SIZE];
  const size_t n = inner_->checkRecvFrame(tmp);

  if (mode_ == Mode::Json && (millis() - json_start_ms_) >= kJsonTimeoutMs) {
    finishJson();
  }

  if (n == 0) return 0;

  if (mode_ == Mode::Idle && tmp[0] == '{') {
    mode_ = Mode::Json;
    json_len_ = 0;
    json_start_ms_ = millis();
    feed(tmp, n);
    return 0;
  }
  if (mode_ == Mode::Json) {
    feed(tmp, n);
    return 0;
  }

  memcpy(dest, tmp, n);
  return n;
}
