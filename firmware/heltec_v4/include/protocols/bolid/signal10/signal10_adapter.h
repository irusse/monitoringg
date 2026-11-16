#pragma once

#include "device/adapter.h"
#include "mesh485/port.h"
#include "mesh485/types.h"
#include "protocols/bolid/orion_frame.h"

// Path B MONITOR: native Orion status-read. No CONTROL this run.
// Probe is the Habr 563584 encrypted status request (cmd 0x57/0x02).
// TX only when CONFIG protocol is BOLID_SIGNAL10 (manual; no AUTO).
class Signal10Adapter : public DeviceAdapter {
public:
  static constexpr uint8_t kDefaultAddr = kOrionDefaultAddr;
  static constexpr uint8_t kDumpAddr = kOrionDumpAddr;
  static constexpr uint8_t kMessageKey = kOrionMessageKey;
  static constexpr uint8_t kCmdStatus = kOrionCmdStatus;
  static constexpr uint8_t kCmdStatusArg = kOrionCmdStatusArg;
  static constexpr size_t kStatusReqLen = kOrionStatusReqLen;
  static constexpr uint32_t kPollIntervalMs = 5000;
  static constexpr uint32_t kTxnTimeoutMs = MESH485_TXN_TIMEOUT_DEFAULT_MS;

  const char* id() const override { return "S10-001"; }
  const char* protocolKey() const override { return "BOLID_SIGNAL10"; }

  void bindPort(Mesh485Port* port) { port_ = port; }
  void setTxBlocked(bool blocked) { tx_blocked_ = blocked; }
  void setSelected(bool on);
  bool selected() const { return selected_; }
  void setAddress(uint8_t addr) { addr_ = addr ? addr : kDefaultAddr; }

  // NORMAL / FAULT (offline → FAULT). No ALARM from undecoded status bytes.
  const char* overlayState() const;

  // Dump-justified status request. addr 3 must match kOrionHabrStatusReq.
  static size_t buildStatusRequest(uint8_t* out, size_t max, uint8_t addr) {
    return orion_build_status_request(out, max, addr);
  }
  static bool frameCrcOk(const uint8_t* frame, size_t len) {
    return orion_frame_crc_ok(frame, len);
  }
  static uint8_t framePeer(const uint8_t* frame) { return orion_frame_peer(frame); }

  bool begin() override;
  bool poll() override;
  bool getInfo(DeviceInfo* out) override;
  size_t getPoints(DevicePoint* out, size_t max) override;
  size_t getEvents(DeviceEvent* out, size_t max) override;

private:
  Mesh485Port* port_ = nullptr;
  bool tx_blocked_ = false;
  bool selected_ = false;
  uint8_t addr_ = kDefaultAddr;
  bool online_ = false;
  bool have_zone1_ = false;
  bool in_flight_ = false;
  uint32_t next_tx_ms_ = kPollIntervalMs;
  uint32_t tx_at_ms_ = 0;
  bool diag_event_ = false;
  uint32_t diag_seq_ = 0;
  char diag_uuid_[DEVICE_EVENT_ID_MAX] = {};

  bool acceptStatusReply(const uint8_t* frame, size_t len);
  void startTxn();
};
