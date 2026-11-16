#pragma once

#include "access/access_controller.h"
#include "access/access_gate.h"
#include "device/registry.h"
#include "mesh485/auth.h"
#include "mesh485/bridge.h"
#include "mesh485/bus_cfg.h"
#include "mesh485/pcn_hop.h"
#include "mesh485/port.h"
#include "mesh485/replay.h"
#include "mesh485/roles.h"
#include "reader/rdm6300.h"
#include "mesh485/types.h"
#include "storage/event_store.h"

class Mesh485Dispatcher {
public:
  Mesh485Dispatcher();

  size_t handleLine(const char* json, const Mesh485Env& env, char* out, size_t out_max);
  void handle(const Mesh485Request& req, const Mesh485Env& env, Mesh485Response* resp);
  const char* lastError() const { return last_error_; }
  void clearReplay() { replay_.clear(); }

  void tick(uint32_t now_s) { bridge_.tick(now_s); }
  void captureRx(const uint8_t* data, size_t len, int64_t ts) { bridge_.capture(data, len, ts); }
  void closeSession() { bridge_.close(); }
  bool sessionOpen() const { return bridge_.isOpen(); }
  const char* sessionLevelName() const { return bridge_.levelName(); }
  Mesh485BridgeLevel sessionLevel() const { return bridge_.level(); }

  DeviceRegistry& devices() { return devices_; }
  const DeviceRegistry& devices() const { return devices_; }
  EventStore& events() { return events_; }
  const EventStore& events() const { return events_; }
  AccessController& acl() { return acl_; }
  const AccessController& acl() const { return acl_; }
  Mesh485Auth& auth() { return auth_; }
  const Mesh485Auth& auth() const { return auth_; }
  AccessGate& gate() { return gate_; }
  const AccessGate& gate() const { return gate_; }
  void bindAccessHw(const AccessHwHooks& hw) { hw_ = hw; }
  void beginStore() {
    loadRole();
    loadChannel();
    events_.begin();
    acl_.begin();
    auth_.begin();
    cfg_.begin();
    devices_.setBusProtocol(cfg_.protocol());
  }
  // Factory default is the build flag. A saved choice in /m485_role.bin wins.
  bool roleSource() const { return role_source_; }
  void setRoleSource(bool on);
  void toggleRole() { setRoleSource(!role_source_); }
  uint8_t channel() const { return channel_; }
  void bumpChannel();
  void fillAlarm(PcnScreen* screen) const;
  bool eventsSubscribed() const { return subscribed_; }
  size_t nextPendingEventLine(char* out, size_t max, uint32_t* sequence);
  bool ackMesh(uint32_t sequence);
  bool failMesh(uint32_t sequence);
  void pollDevices() {
    devices_.pcn().setListen(role_source_);
    devices_.setTxBlocked(rdm_mode_ || bridge_.isOpen());
    devices_.pollAll();
    harvestEvents();
  }
  // Monitor image (-D MESH485_RDM6300): UART1 is the reader, DE stays RX.
  void setRdmMode(bool on) { rdm_mode_ = on; }
  bool rdmMode() const { return rdm_mode_; }
  void setRdmIgnore(bool on) { rdm_ignore_ = on; }
  uint32_t rdmErrorCount() const { return rdm_.errorCount(); }
  // Same ACL path as a Wiegand card. Deny does not pulse the gate.
  bool acceptCard(const char* uid, int64_t ts, const char* direction, bool actuate, bool* opened);
  // Feed UART bytes. One accepted frame becomes acceptCard. Repeats inside 2 s do not.
  bool pushRdm(const uint8_t* data, size_t len, uint32_t now_ms, const char* direction, bool actuate,
               bool* opened);
  void setNodeId(const char* id);
  const char* nodeId() const { return node_id_; }
  // MeshCore node name. Sent with the hop and drawn as the alarm line.
  void setAlarmName(const char* name);
  // Source: last local edge. Receiver: ALARM while any sender's frame is younger than 2 min.
  const char* pcnShownState() const;
  // Node id on the alarm screen: this node for a local edge, the sender for a hop.
  const char* pcnFromNode() const;
  // Two short lines for the СИГНАЛИЗАЦИЯ screen.
  uint16_t pcnAlarmGen() const { return pcn_alarm_gen_; }
  // Next pending PCN frame, or 0 while the backoff for that uuid has not elapsed.
  size_t pollPcnHop(uint8_t* out, size_t max, uint32_t now_ms);
  // State frame from another node → EventStore source LORA + ack bytes.
  // Ack frame → mesh_ok on the matching uuid. Own echo is ignored.
  bool acceptPcnFrame(const uint8_t* frame, size_t len, uint8_t* ack, size_t* ack_len);

private:
  Mesh485Replay replay_;
  Mesh485Auth auth_;
  Mesh485BusCfg cfg_;
  Mesh485Bridge bridge_;
  DeviceRegistry devices_;
  EventStore events_;
  AccessController acl_;
  AccessGate gate_;
  AccessHwHooks hw_;
  Rdm6300Parser rdm_;
  bool rdm_mode_ = false;
  bool rdm_ignore_ = false;
  static constexpr int kHoldSlots = 16;
  static constexpr int kHistSlots = 8;
  struct PcnRxHold {
    char node[8];
    char label[32];
    uint32_t last_ms;
    uint32_t start_ms;
    uint8_t used;
  };
  struct PcnHist {
    char name[24];
    uint32_t start_ms;
  };

  char node_id_[16] = MESH485_NODE_ID;
  char alarm_name_[32] = {};
  char pcn_shown_[8] = {};
  char pcn_from_[8] = {};
  uint16_t pcn_alarm_gen_ = 0;
  char hop_wait_uuid_[40] = {};
  uint32_t hop_next_ms_ = 0;
  uint32_t beacon_next_ms_ = 0;
#ifndef MESH485_ROLE_SOURCE
#define MESH485_ROLE_SOURCE 1
#endif
  bool role_source_ = MESH485_ROLE_SOURCE != 0;
  uint8_t channel_ = 1;
  PcnRxHold holds_[kHoldSlots] = {};
  PcnHist hist_[kHistSlots] = {};
  uint8_t hist_n_ = 0;
  char last_error_[32] = {};
  bool busy_ = false;
  bool subscribed_ = false;
  bool last_txn_mesh_ok_ = false;
  bool last_txn_device_ok_ = false;
  Mesh485Role caller_ = Mesh485Role::Viewer;

  void setError(const char* e);
  void fillMeta(const Mesh485Request& req, const Mesh485Env& env, Mesh485Response* resp);
  void refuseUnknownDevice(Mesh485Response* resp);
  void refuseBadAuth(Mesh485Response* resp);
  void opAuthSet(const Mesh485Request& req, Mesh485Response* resp);
  DeviceAdapter* resolveDevice(const Mesh485Request& req, bool required);
  void harvestEvents();
  void loadRole();
  void saveRole() const;
  void loadChannel();
  void saveChannel() const;
  void pushHist(const char* name, uint32_t start_ms);
  bool noteHold(const char* node, uint32_t now_ms, const char* label = nullptr);
  bool holdLive(uint32_t now_ms) const;
  const char* liveHoldNode(uint32_t now_ms) const;
  void injectStubCard();
  static bool sinceMatch(const EventRecord& e, const char* since, bool* include);
  void opNodeStatus(const Mesh485Env& env, Mesh485Response* resp);
  void opConfigGet(const Mesh485Env& env, Mesh485Response* resp);
  void opConfigSet(const Mesh485Request& req, const Mesh485Env& env, Mesh485Response* resp);
  void opDiagnostics(const Mesh485Env& env, Mesh485Response* resp);
  void opRs485Transaction(const Mesh485Request& req, const Mesh485Env& env,
                          Mesh485Response* resp);
  void opBridgeOpen(const Mesh485Request& req, const Mesh485Env& env, Mesh485Response* resp);
  void opBridgeClose(const Mesh485Env& env, Mesh485Response* resp);
  void opBridgePoll(const Mesh485Env& env, Mesh485Response* resp);
  void opDeviceDiscover(Mesh485Response* resp);
  void opDeviceInfo(const Mesh485Request& req, Mesh485Response* resp);
  void opDeviceStatus(const Mesh485Request& req, Mesh485Response* resp);
  void opPointList(const Mesh485Request& req, Mesh485Response* resp);
  void opPointStatus(const Mesh485Request& req, Mesh485Response* resp);
  void opEventList(const Mesh485Request& req, Mesh485Response* resp);
  void opEventSubscribe(const Mesh485Request& req, Mesh485Response* resp);
  void opCommandExecute(const Mesh485Request& req, const Mesh485Env& env,
                        Mesh485Response* resp);
  void opAccess(const Mesh485Request& req, const Mesh485Env& env, Mesh485Response* resp);
  void finishAccess(Mesh485Response* resp, const char* err, bool mesh_ok, bool device_ok,
                    const char* extra_json);

  struct LastCommand {
    bool used = false;
    char optr[24] = {};
    char device_id[16] = {};
    char capability[24] = {};
    char point_id[16] = {};
    bool mesh_ok = false;
    bool device_ok = false;
    char error[24] = {};
  } last_cmd_;
};
