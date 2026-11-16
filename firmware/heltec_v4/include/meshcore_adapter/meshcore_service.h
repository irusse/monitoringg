#pragma once

#include <stddef.h>
#include <stdint.h>

enum class MeshIndicator { Active, Inactive, Error };

struct MeshDiagnostics {
  MeshIndicator indicator = MeshIndicator::Inactive;
  bool radio_ok = false;
  bool repeater_enabled = false;
  int rssi = 0;
  float snr = 0;
  uint32_t rx = 0;
  uint32_t tx = 0;
  uint32_t relayed = 0;
};

class MeshCoreService {
public:
  // Power / FEM. Call before Access OLED. Do not claim periph_power:
  // on Heltec V4 that drives GPIO36 HIGH and blanks the SSD1306.
  void prepareBoard();
  bool start();
  void stop();
  void loop();
  MeshIndicator indicator() const;
  MeshDiagnostics diagnostics() const;
  void setRepeaterEnabled(bool on);
  bool repeaterEnabled() const;
  bool send(const uint8_t* data, size_t len);
  bool poll(uint8_t* buf, size_t* len, size_t max_len);

  // USB + BLE mux: JSON lines starting with '{' are MESH 485, not Companion.
  typedef size_t (*Mesh485JsonFn)(const char* line, char* out, size_t out_max, void* ctx);
  void setMesh485Handler(Mesh485JsonFn fn, void* ctx);
  // Unsolicited NDJSON (EVENT_SUBSCRIBE). USB CDC + BLE NUS; not Companion frames.
  size_t writeMesh485Line(const char* line, size_t n);

  const char* bleName() const { return ble_name_; }
  uint32_t blePin() const { return ble_pin_; }
  // Name edited in the MeshCore app. This is the alarm line.
  const char* nodeName() const;

private:
  bool board_ready_ = false;
  bool running_ = false;
  bool radio_ok_ = false;
  bool repeater_ = true;
  char ble_name_[48] = "3MESH-TS-001";
  uint32_t ble_pin_ = 123456;
};
