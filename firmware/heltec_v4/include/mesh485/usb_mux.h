#pragma once

#include <Arduino.h>
#include "mesh485/types.h"

// USB CDC mux: JSON MESH 485 lines starting with '{' never enter Companion.
// All other bytes pass through to ArduinoSerialInterface.
class UsbMuxStream : public Stream {
public:
  typedef size_t (*JsonHandler)(const char* line, char* out, size_t out_max, void* ctx);

  void begin(Stream& inner);
  void setHandler(JsonHandler fn, void* ctx);
  void pump();  // drain Serial before Companion checkRecvFrame

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t n) override;

private:
  static constexpr size_t kCompCap = 512;
  static constexpr size_t kJsonMax = MESH485_JSON_MAX;
  static constexpr uint32_t kJsonTimeoutMs = 50;

  Stream* inner_ = nullptr;
  JsonHandler handler_ = nullptr;
  void* handler_ctx_ = nullptr;

  uint8_t comp_[kCompCap] = {};
  size_t comp_head_ = 0;
  size_t comp_count_ = 0;

  enum class Mode : uint8_t { Idle, Json };
  Mode mode_ = Mode::Idle;
  char json_[kJsonMax] = {};
  size_t json_len_ = 0;
  uint32_t json_start_ms_ = 0;

  bool compPush(uint8_t c);
  int compPeek() const;
  int compPop();
  void finishJson(bool timeout);
};
