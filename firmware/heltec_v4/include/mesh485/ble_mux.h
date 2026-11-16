#pragma once

#include "mesh485/usb_mux.h"
#include <helpers/esp32/SerialBLEInterface.h>

// BLE NUS mux: a frame whose first byte is '{' is MESH 485 NDJSON (same
// rule as UsbMuxStream). Do not feed those bytes to Companion. Responses
// are raw JSON + '\n', chunked to MAX_FRAME_SIZE — not 0x3c/0x3e Companion
// frames.
class BleMuxInterface : public BaseSerialInterface {
public:
  using JsonHandler = UsbMuxStream::JsonHandler;

  void bind(SerialBLEInterface& inner);
  void setHandler(JsonHandler fn, void* ctx);
  size_t writeJsonLine(const char* line, size_t n);

  void enable() override;
  void disable() override;
  bool isEnabled() const override;
  bool isConnected() const override;
  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;

private:
  static constexpr size_t kJsonMax = MESH485_JSON_MAX;
  static constexpr uint32_t kJsonTimeoutMs = 50;

  SerialBLEInterface* inner_ = nullptr;
  JsonHandler handler_ = nullptr;
  void* handler_ctx_ = nullptr;

  enum class Mode : uint8_t { Idle, Json };
  Mode mode_ = Mode::Idle;
  char json_[kJsonMax] = {};
  size_t json_len_ = 0;
  uint32_t json_start_ms_ = 0;

  void feed(const uint8_t* buf, size_t n);
  void finishJson();
};
