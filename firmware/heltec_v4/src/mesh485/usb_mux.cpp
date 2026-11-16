#include "mesh485/usb_mux.h"

#include <string.h>

void UsbMuxStream::begin(Stream& inner) {
  inner_ = &inner;
  mode_ = Mode::Idle;
  json_len_ = 0;
  comp_head_ = 0;
  comp_count_ = 0;
}

void UsbMuxStream::setHandler(JsonHandler fn, void* ctx) {
  handler_ = fn;
  handler_ctx_ = ctx;
}

bool UsbMuxStream::compPush(uint8_t c) {
  if (comp_count_ >= kCompCap) return false;
  const size_t i = (comp_head_ + comp_count_) % kCompCap;
  comp_[i] = c;
  ++comp_count_;
  return true;
}

int UsbMuxStream::compPeek() const {
  if (comp_count_ == 0) return -1;
  return comp_[comp_head_];
}

int UsbMuxStream::compPop() {
  if (comp_count_ == 0) return -1;
  const int c = comp_[comp_head_];
  comp_head_ = (comp_head_ + 1) % kCompCap;
  --comp_count_;
  return c;
}

void UsbMuxStream::finishJson(bool /*timeout*/) {
  json_[json_len_ < kJsonMax ? json_len_ : kJsonMax - 1] = 0;
  if (json_len_ > 0 && handler_) {
    static char out[MESH485_RESP_MAX];
    const size_t n = handler_(json_, out, sizeof(out), handler_ctx_);
    if (n && inner_) {
      inner_->write((const uint8_t*)out, n);
      inner_->write((const uint8_t*)"\n", 1);
    }
  }
  mode_ = Mode::Idle;
  json_len_ = 0;
}

void UsbMuxStream::pump() {
  if (!inner_) return;

  while (inner_->available()) {
    if (mode_ == Mode::Idle) {
      const int p = inner_->peek();
      if (p == '{') {
        mode_ = Mode::Json;
        json_len_ = 0;
        json_start_ms_ = millis();
      } else {
        const int c = inner_->read();
        if (c < 0) break;
        if (!compPush((uint8_t)c)) break;
        continue;
      }
    }

    if (mode_ == Mode::Json) {
      const int c = inner_->read();
      if (c < 0) break;
      if (c == '\n' || json_len_ >= kJsonMax - 1) {
        if (c != '\n' && c != '\r' && json_len_ < kJsonMax - 1) {
          json_[json_len_++] = (char)c;
        }
        finishJson(false);
      } else if (c != '\r') {
        json_[json_len_++] = (char)c;
      }
    }
  }

  if (mode_ == Mode::Json && (millis() - json_start_ms_) >= kJsonTimeoutMs) {
    finishJson(true);
  }
}

int UsbMuxStream::available() { return (int)comp_count_; }

int UsbMuxStream::read() { return compPop(); }

int UsbMuxStream::peek() { return compPeek(); }

void UsbMuxStream::flush() {
  if (inner_) inner_->flush();
}

size_t UsbMuxStream::write(uint8_t c) {
  if (!inner_) return 0;
  return inner_->write(c);
}

size_t UsbMuxStream::write(const uint8_t* buf, size_t n) {
  if (!inner_ || !buf) return 0;
  return inner_->write(buf, n);
}
