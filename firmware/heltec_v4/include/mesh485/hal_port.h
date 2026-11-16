#pragma once

#include "hal/rs485.h"
#include "mesh485/port.h"

class Rs485HalPort : public Mesh485Port {
public:
  explicit Rs485HalPort(Rs485Hal& h) : h_(&h) {}

  size_t writeFrame(const uint8_t* data, size_t len) override {
    return h_->writeFrame(data, len);
  }
  size_t readFrame(uint8_t* buf, size_t max, uint32_t timeout_ms) override {
    return h_->readFrame(buf, max, timeout_ms);
  }
  uint32_t rxCount() const override { return h_->rxCount(); }
  uint32_t txCount() const override { return h_->txCount(); }
  uint32_t baud() const override { return h_->baud(); }
  char parity() const override { return h_->parityChar(); }
  uint8_t stopBits() const override { return h_->stopBits(); }
  bool ok() const override { return h_->ok(); }
  void setDirection(bool tx) override { h_->setDirection(tx); }
  bool txDirection() const override { return h_->txDirection(); }

private:
  Rs485Hal* h_;
};
