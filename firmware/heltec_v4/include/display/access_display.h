#pragma once

#include <Arduino.h>
#include "meshcore_adapter/meshcore_service.h"
#include "mesh485/pcn_hop.h"

class AccessDisplay {
public:
  bool begin();
  bool reinit();
  void loop();
  void attachRadio(MeshCoreService* radio);

  void showReady(const char* device_id, MeshIndicator mesh, uint16_t users);
  void showGranted(const char* name, const char* time);
  void showGranted(const char* name, const char* direction, const char* time);
  void showDenied(const char* reason, const char* time);
  void showRemote(const char* cmd, const char* operator_id);
  void showLocked();
  void showEmergency();
  void showMeshDiagnostics(const MeshDiagnostics& d);
  void setRs485Status(const char* line, bool wake = false);
  // READY stub: "PP-001 FAULT" / "S10-001 FAULT" / "S20-001 FAULT" (CONFIG) / NORMAL.
  void setPpStatus(const char* line);
  // V4: READY row, or a full-screen alarm while the contact/hop says ALARM.
  // V3: full-screen alarm for 40 s after a new generation, then НОРМА if no new signal.
  void setPcnStatus(const char* line, const char* node, uint16_t alarm_gen);
  // СИГНАЛИЗАЦИЯ screen: channel, live count, two alarm lines, history.
  void setAlarmText(const PcnScreen& screen);
  void setRoleHandlers(bool (*is_source)(void*), void (*toggle)(void*), void* ctx);
  void setChannelHandlers(uint8_t (*get)(void*), void (*bump)(void*), void* ctx);
  void setUsers(uint16_t n) { users_ = n; }
  void setPendingCount(uint16_t n);
  void showBridge(const char* level);
  void setBridgeAbortHandler(void (*fn)(void*), void* ctx);

  // Blank after 10 s idle. USER press wakes (that press is not a menu/demo).
  // Access events and emergency still wake the panel.
  // While a bridge session is on screen the panel stays awake.
  void noteActivity();
  bool isAsleep() const { return asleep_; }
  bool bridgeActive() const { return bridge_active_; }

private:
  enum class Screen : uint8_t {
    Ready,
    Granted,
    Denied,
    Remote,
    Locked,
    Emergency,
    Menu,
    Info,
    Bridge,
    Alarm,
    History,
  };

  enum class MenuItem : uint8_t {
    Info = 0,
    Repeater = 1,
    Alarm = 2,
    Role = 3,
    Channel = 4,
    History = 5,
    Back = 6,
    Count = 7
  };

  MeshCoreService* radio_ = nullptr;
  bool begun_ = false;
  bool panel_ok_ = false;
  Screen screen_ = Screen::Alarm;
  MeshIndicator mesh_ = MeshIndicator::Active;
  bool repeater_ = false;
  uint16_t users_ = 0;
  char device_id_[16] = "TS-001";
  char line2_[24] = {};
  char line3_[24] = {};
  char last_uid_[24] = "--";
  char rs485_line_[20] = "RS485 idle";
  char pp_line_[20] = "PP-001 FAULT";
  char pcn_line_[20] = "НОРМА";
  char pcn_node_[16] = {};
  char alarm_a_[40] = {};
  char alarm_b_[40] = {};
  char alarm_hist_[3][32] = {};
  uint8_t alarm_hist_n_ = 0;
  uint8_t alarm_ch_ = 1;
  uint8_t alarm_n_ = 0;
  bool role_source_ui_ = true;
  bool (*role_is_)(void*) = nullptr;
  void (*role_toggle_)(void*) = nullptr;
  void* role_ctx_ = nullptr;
  uint8_t (*channel_get_)(void*) = nullptr;
  void (*channel_bump_)(void*) = nullptr;
  void* channel_ctx_ = nullptr;
  bool led_forced_ = false;
  bool lamp_forced_ = false;
  bool alarm_locked_ = false;
  uint16_t alarm_acked_gen_ = 0;
  uint8_t click_n_ = 0;
  uint32_t click_at_ = 0;
  uint32_t chrome_at_ = 0;
  uint8_t batt_pct_ = 0;
  uint16_t pending_q_ = 0;
  uint8_t demo_step_ = 0;
  uint8_t menu_sel_ = 0;
  bool transient_ = false;
  uint32_t shown_at_ = 0;
  uint32_t demo_at_ = 0;
  uint32_t boot_ms_ = 0;
  uint32_t activity_at_ = 0;
  bool asleep_ = false;
  uint8_t alarm_blinks_ = 0;
  bool alarm_dark_ = false;
  bool alarm_show_ = false;
  uint32_t alarm_blink_at_ = 0;
  uint32_t alarm_until_ = 0;
  uint32_t vibe_until_ = 0;
  uint16_t alarm_seen_ = 0;
  bool swallow_press_ = false;
  bool bridge_active_ = false;
  char bridge_level_[16] = {};
  void (*bridge_abort_)(void*) = nullptr;
  void* bridge_abort_ctx_ = nullptr;

  bool btn_down_ = false;
  bool long_fired_ = false;
  uint32_t btn_at_ = 0;

  void powerOled();
  bool initPanel();
  bool probePanel();
  void pollButton();
  void sleepPanel();
  void wakePanel();
  void maybeSleep();
  void startAlarmBlink();
  void tickAlarmBlink();
  void tickAlarmLed();
  void tickAlarmLamp();
  void tickChrome();
  void advanceCycle();
  int cycleIndex() const;
  void drawBattery();
  void drawSweep();
  void drawScreenNo();
  void drawAlarmOverlay();
  void tickAlarmHold();
  void endAlarmShow();
  void startVibe();
  void tickVibe();
  void tickSiren();
  bool siren_on_ = false;
  void onShortPress();
  void onLongPress();
  void cycleDemo();
  void enterMenu();
  void activateMenuItem();
  void toggleRepeater();
  void render();
  void drawMeshIndicator();
};
