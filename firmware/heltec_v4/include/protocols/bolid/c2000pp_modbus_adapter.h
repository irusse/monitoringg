#pragma once

#include "device/adapter.h"
#include "mesh485/port.h"
#include "mesh485/types.h"

#ifndef MESH485_ALLOW_PP_RELAY
#define MESH485_ALLOW_PP_RELAY 0
#endif

// Path A: Heltec is Modbus-RTU client, С2000-ПП is slave.
// Probe: FC 0x03 start 46152 (0xB448) qty 2 (type + version).
// CONTROL_RELAY only for named R1..R8 when panel allow-list says so.
// Never a generic fire-disable / disarm-all. Never IR 3600.
class C2000ppModbusAdapter : public DeviceAdapter {
public:
  static constexpr uint8_t kDefaultSlave = 1;
  static constexpr uint16_t kProbeStart = 46152;  // table 1.1.5.14.1
  static constexpr uint16_t kProbeQty = 2;
  static constexpr uint32_t kPollIntervalMs = 5000;
  static constexpr uint32_t kTxnTimeoutMs = MESH485_TXN_TIMEOUT_DEFAULT_MS;
  static constexpr uint8_t kMaxNamedRelays = 8;
  static constexpr uint16_t kRelay1Coil = 0x2710;  // coil 10000 = relay №1

  const char* id() const override { return "PP-001"; }
  const char* protocolKey() const override { return "BOLID_C2000PP"; }

  void bindPort(Mesh485Port* port) { port_ = port; }
  void setTxBlocked(bool blocked) { tx_blocked_ = blocked; }
  void setSelected(bool on);
  bool selected() const { return selected_; }
  void setSlave(uint8_t addr) { slave_ = addr ? addr : kDefaultSlave; }

  // Panel config allow-list: named relay №n (1..8) → point Rn. Default none.
  void allowRelay(uint8_t n, bool on);
  bool relayAllowed(uint8_t n) const;
  bool anyRelayAllowed() const;

  // NORMAL / FAULT / ALARM for the READY overlay (offline → FAULT).
  const char* overlayState() const;

  bool begin() override;
  bool poll() override;
  bool getInfo(DeviceInfo* out) override;
  size_t getPoints(DevicePoint* out, size_t max) override;
  size_t getEvents(DeviceEvent* out, size_t max) override;
  bool execute(const CommandRequest& req, CommandOutcome* out) override;

private:
  Mesh485Port* port_ = nullptr;
  bool tx_blocked_ = false;
  bool selected_ = true;
  uint8_t slave_ = kDefaultSlave;
  bool online_ = false;
  bool alarm_ = false;
  bool in_flight_ = false;
  uint32_t next_tx_ms_ = kPollIntervalMs;
  uint32_t tx_at_ms_ = 0;
  uint16_t type_reg_ = 0;
  uint16_t version_reg_ = 0;
  uint16_t zone1_code_ = 0;
  bool have_zone1_ = false;
  uint8_t pending_kind_ = 0;  // 0 = probe type/ver, 1 = zone 1
  bool diag_event_ = false;
  uint32_t diag_seq_ = 0;
  char diag_uuid_[DEVICE_EVENT_ID_MAX] = {};
  bool relay_allow_[kMaxNamedRelays] = {};
  char relay_state_[kMaxNamedRelays][8] = {};

  size_t buildReadHolding(uint8_t* adu, size_t max, uint16_t start, uint16_t qty) const;
  size_t buildWriteCoil(uint8_t* adu, size_t max, uint16_t coil, bool on) const;
  bool acceptFc03(const uint8_t* adu, size_t len);
  bool acceptFc05(const uint8_t* adu, size_t len) const;
  void startTxn(uint8_t kind);
  const char* zoneStateName(uint16_t code) const;
  static bool parseRelayPoint(const char* point_id, uint8_t* n_out);
  static bool parseOnOff(const char* action, bool* on_out);
};
