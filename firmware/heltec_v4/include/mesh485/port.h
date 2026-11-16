#pragma once

#include "mesh485/types.h"
#include <stddef.h>
#include <stdint.h>

// Abstract RS-485 port for the MESH 485 dispatcher (HAL on device, loopback in tests).
class Mesh485Port {
public:
  virtual ~Mesh485Port() {}
  virtual size_t writeFrame(const uint8_t* data, size_t len) = 0;
  virtual size_t readFrame(uint8_t* buf, size_t max, uint32_t timeout_ms) = 0;
  virtual uint32_t rxCount() const = 0;
  virtual uint32_t txCount() const = 0;
  virtual uint32_t baud() const = 0;
  virtual char parity() const = 0;  // 'N' / 'E' / 'O'
  virtual uint8_t stopBits() const = 0;
  virtual bool ok() const = 0;
  // DE/RE: true = TX. Close/timeout must leave this false (RX fail-safe).
  virtual void setDirection(bool tx) { (void)tx; }
  virtual bool txDirection() const { return false; }
};

struct Mesh485Env {
  Mesh485Port* port = nullptr;
  bool radio_ok = false;
  bool repeater = false;
  int rssi = 0;
  float snr = 0;
  uint32_t mesh_rx = 0;
  uint32_t mesh_tx = 0;
  uint32_t uptime_s = 0;
  uint32_t heap = 0;
  int64_t unix_now = 0;  // 0 = skip stale-ts wall-clock check
  const char* node_id = MESH485_NODE_ID;
  const char* source = "USB";  // ACCESS_OPEN event source: USB | BLE | LORA
  uint8_t sender_pub[32] = {};  // optional MeshCore sender; empty unless sender_pub_set
  bool sender_pub_set = false;
};
