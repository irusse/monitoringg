#include "display/access_display.h"
#include "display/alarm_ui.h"
#include "display/oled_text.h"
#include "access/pins.h"
#include <string.h>
#include <stdio.h>

#ifndef ENABLE_USB_INTERFACE
#define OLED_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define OLED_LOG(...) ((void)0)
#endif

#ifndef ACCESS_DISPLAY_SERIAL_ONLY
#include <Wire.h>
#include <Adafruit_GFX.h>
#ifdef HELTEC_LORA_V4_TFT
#include <SPI.h>
#include <Adafruit_ST7789.h>
#ifndef PIN_TFT_MISO
#define PIN_TFT_MISO -1
#endif
#ifndef PIN_TFT_LEDA_CTL
#define PIN_TFT_LEDA_CTL 21
#endif
static SPIClass g_tft_spi(HSPI);
class AccessTft : public Adafruit_ST7789 {
 public:
  AccessTft() : Adafruit_ST7789(&g_tft_spi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST) {}
  void clearDisplay() { fillScreen(ST77XX_BLACK); }
  void display() {}
  void ssd1306_command(uint8_t cmd) {
    if (PIN_TFT_LEDA_CTL < 0) return;
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_LEDA_CTL, cmd == 0xAE ? LOW : HIGH);
  }
};
static AccessTft g_oled;
#define SSD1306_WHITE ST77XX_WHITE
#define SSD1306_BLACK ST77XX_BLACK
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#else
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>
static Adafruit_SSD1306 g_oled(128, 64, &Wire, PIN_OLED_RST);
#endif
#endif

static constexpr uint32_t kBlankAfterMs = 50000;
static constexpr uint32_t kStrobeMs = 60;
static constexpr uint32_t kVibeMs = 5000;

static void copyZ(char* dst, size_t n, const char* src) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

void AccessDisplay::attachRadio(MeshCoreService* radio) { radio_ = radio; }

void AccessDisplay::powerOled() {
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  // PMOS: LOW enables the OLED (and its I2C pull-ups). Do not use
  // PIN_VEXT_EN_ACTIVE here — MeshCore's HIGH would cut the rail.
  pinMode(PIN_VEXT_EN, OUTPUT);
  digitalWrite(PIN_VEXT_EN, PIN_VEXT_OLED_ON);
  delay(120);
#endif
}

#ifndef ACCESS_DISPLAY_SERIAL_ONLY
bool AccessDisplay::probePanel() {
#ifdef HELTEC_LORA_V4_TFT
  return true;
#else
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() == 0) return true;
  Wire.beginTransmission(0x3D);
  return Wire.endTransmission() == 0;
#endif
}

bool AccessDisplay::initPanel() {
#ifdef HELTEC_LORA_V4_TFT
  powerOled();
  if (PIN_TFT_LEDA_CTL >= 0) {
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
  }
  g_tft_spi.begin(PIN_TFT_SCL, PIN_TFT_MISO, PIN_TFT_SDA, PIN_TFT_CS);
  g_oled.init(240, 320);
  g_oled.setRotation(3);
  g_oled.setSPISpeed(40000000);
  g_oled.fillScreen(ST77XX_BLACK);
  g_oled.setTextWrap(false);
  g_oled.cp437(true);
  g_oled.setTextColor(ST77XX_WHITE);
  panel_ok_ = true;
  Serial.println("TFT ready");
  return true;
#else
  powerOled();
  Wire.end();
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  delay(20);

  // periphBegin=false: Adafruit must not Wire.begin() on variant SDA/SCL 3/4.
  const bool started = g_oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
  panel_ok_ = started && probePanel();
  if (!panel_ok_) {
    Serial.println("OLED begin failed (VEXT must be LOW / I2C 17/18/21)");
    return false;
  }
  g_oled.ssd1306_command(SSD1306_DISPLAYON);
  g_oled.setTextWrap(false);
  g_oled.cp437(true);
  return true;
#endif
}
#else
bool AccessDisplay::probePanel() { return false; }
bool AccessDisplay::initPanel() { return true; }
#endif

bool AccessDisplay::begin() {
  boot_ms_ = millis();
  activity_at_ = boot_ms_;
  asleep_ = false;
  pinMode(PIN_USER_BTN, INPUT_PULLUP);
  pinMode(PIN_VIBE, OUTPUT);
  digitalWrite(PIN_VIBE, PIN_VIBE_ON == HIGH ? LOW : HIGH);
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  begun_ = true;
  panel_ok_ = false;
  Serial.println("OLED serial-only (no SSD1306 in this env)");
  return true;
#else
  begun_ = true;
  demo_at_ = millis();
  if (!initPanel()) return false;
  render();
  Serial.println("OLED Access UI ready");
  return true;
#endif
}

bool AccessDisplay::reinit() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  return true;
#else
  if (!begun_) return begin();
  const bool ok = initPanel();
  if (ok) {
    render();
    Serial.println("OLED reinit ok");
  }
  return ok;
#endif
}

void AccessDisplay::tickAlarmLed() {
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  if (!alarm_show_) {
    if (led_forced_) {
      digitalWrite(PIN_ALARM_LED, LOW);
      led_forced_ = false;
    }
    return;
  }
  if (!led_forced_) pinMode(PIN_ALARM_LED, OUTPUT);
  led_forced_ = true;
  // 250 ms on, 250 ms off = 2 Hz.
  const bool on = ((millis() / 250u) & 1u) == 0;
  digitalWrite(PIN_ALARM_LED, on ? HIGH : LOW);
#else
  (void)led_forced_;
#endif
}

void AccessDisplay::tickAlarmLamp() {
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  // Receiver only. The line blink and this pin share the 2 Hz phase.
  const bool on = alarm_show_ && !role_source_ui_ && ((millis() / 250u) & 1u) == 0;
  if (!alarm_show_ || role_source_ui_) {
    if (lamp_forced_) {
      digitalWrite(PIN_ALARM_LAMP, LOW);
      lamp_forced_ = false;
    }
    return;
  }
  if (!lamp_forced_) pinMode(PIN_ALARM_LAMP, OUTPUT);
  lamp_forced_ = true;
  digitalWrite(PIN_ALARM_LAMP, on ? HIGH : LOW);
#else
  (void)lamp_forced_;
#endif
}

void AccessDisplay::loop() {
  if (!begun_) return;
  tickAlarmLed();
  tickAlarmLamp();
  tickChrome();
  pollButton();
  tickAlarmHold();
  tickAlarmBlink();
  tickVibe();
  tickSiren();
  maybeSleep();

#ifdef OLED_DEMO
  if (!asleep_ && screen_ != Screen::Menu && screen_ != Screen::Info) {
    if (millis() - demo_at_ >= 4000) {
      demo_at_ = millis();
      cycleDemo();
    }
  }
#endif

  if (!asleep_ && transient_ && (millis() - shown_at_ >= 3000)) {
    transient_ = false;
    screen_ = bridge_active_ ? Screen::Bridge : Screen::Ready;
    render();
  }
}

void AccessDisplay::pollButton() {
  if (millis() - boot_ms_ < 400) return;
  const bool down = digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED;
  const uint32_t now = millis();

  if (down && !btn_down_) {
    btn_down_ = true;
    long_fired_ = false;
    btn_at_ = now;
    if (asleep_) {
      wakePanel();
      swallow_press_ = true;
    }
    return;
  }

  if (down && btn_down_ && !long_fired_ && !swallow_press_ && (now - btn_at_ >= 1500)) {
    long_fired_ = true;
    onLongPress();
    return;
  }

  if (!down && btn_down_) {
    btn_down_ = false;
    if (swallow_press_) {
      swallow_press_ = false;
      return;
    }
    if (!long_fired_ && (now - btn_at_ >= 30)) {
      onShortPress();
    }
  }
}

void AccessDisplay::noteActivity() { activity_at_ = millis(); }

void AccessDisplay::sleepPanel() {
  if (asleep_ || screen_ == Screen::Emergency) return;
  asleep_ = true;
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  if (panel_ok_) g_oled.ssd1306_command(SSD1306_DISPLAYOFF);
#endif
}

void AccessDisplay::wakePanel() {
  noteActivity();
  asleep_ = false;
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  pinMode(PIN_VEXT_EN, OUTPUT);
  digitalWrite(PIN_VEXT_EN, PIN_VEXT_OLED_ON);
  if (!panel_ok_ && begun_) initPanel();
  if (panel_ok_) {
    g_oled.ssd1306_command(SSD1306_DISPLAYON);
    render();
  }
#endif
}

void AccessDisplay::startVibe() {
  pinMode(PIN_VIBE, OUTPUT);
  digitalWrite(PIN_VIBE, PIN_VIBE_ON);
  vibe_until_ = millis() + kVibeMs;
}

void AccessDisplay::tickVibe() {
  if (vibe_until_ == 0) return;
  if ((int32_t)(millis() - vibe_until_) < 0) return;
  digitalWrite(PIN_VIBE, PIN_VIBE_ON == HIGH ? LOW : HIGH);
  vibe_until_ = 0;
}

void AccessDisplay::tickSiren() {
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  // Receiver only. A source must stay quiet when another node alarms.
  const bool on = alarm_show_ && !role_source_ui_;
  if (!on) {
    if (!siren_on_) return;
    ledcWriteTone(7, 0);
    ledcDetachPin(PIN_VIBE);
    pinMode(PIN_VIBE, OUTPUT);
    digitalWrite(PIN_VIBE, LOW);
    siren_on_ = false;
    return;
  }
  if (!siren_on_) {
    pinMode(PIN_VIBE, OUTPUT);
    ledcSetup(7, 900, 8);
    ledcAttachPin(PIN_VIBE, 7);
    siren_on_ = true;
  }
  const bool hi = ((millis() / 250u) & 1u) == 0;
  ledcWriteTone(7, hi ? 1800 : 900);
#else
  (void)siren_on_;
#endif
}

void AccessDisplay::startAlarmBlink() {
  alarm_blinks_ = 1;
  alarm_dark_ = false;
  alarm_blink_at_ = millis();
  wakePanel();
  if (role_source_ui_) startVibe();
#ifdef MESH485_DEBUG
  Serial.println("OLED alarm strobe");
#endif
}

void AccessDisplay::tickAlarmBlink() {}

void AccessDisplay::endAlarmShow() {
  alarm_show_ = false;
  alarm_locked_ = false;
  alarm_until_ = 0;
  alarm_blinks_ = 0;
  alarm_dark_ = false;
  copyZ(pcn_line_, sizeof(pcn_line_), "НОРМА");
#ifdef MESH485_DEBUG
  Serial.println("OLED alarm off");
#endif
  if (!begun_ || asleep_) return;
  render();
}

void AccessDisplay::tickAlarmHold() {
#ifdef HELTEC_LORA_V3
  if (!alarm_show_ || alarm_until_ == 0) return;
  if ((int32_t)(millis() - alarm_until_) < 0) return;
  endAlarmShow();
#else
  (void)0;
#endif
}

void AccessDisplay::maybeSleep() {
  if (!begun_ || asleep_ || alarm_blinks_ != 0 || alarm_show_) return;
  if (screen_ == Screen::Emergency) return;
  // Stay awake while a bridge session is open (READ_ONLY / RAW / CONTROL).
  if (bridge_active_ && screen_ == Screen::Bridge) return;
  if (millis() - activity_at_ >= kBlankAfterMs) {
    sleepPanel();
  }
}

void AccessDisplay::setPpStatus(const char* line) {
  copyZ(pp_line_, sizeof(pp_line_), line && line[0] ? line : "PP-001 FAULT");
  if (begun_ && !asleep_ && screen_ == Screen::Ready) render();
}

void AccessDisplay::setRoleHandlers(bool (*is_source)(void*), void (*toggle)(void*), void* ctx) {
  role_is_ = is_source;
  role_toggle_ = toggle;
  role_ctx_ = ctx;
}

void AccessDisplay::setChannelHandlers(uint8_t (*get)(void*), void (*bump)(void*), void* ctx) {
  channel_get_ = get;
  channel_bump_ = bump;
  channel_ctx_ = ctx;
}

void AccessDisplay::setAlarmText(const PcnScreen& screen) {
  char next_a[40];
  char next_b[40];
  copyZ(next_a, sizeof(next_a), screen.line_a);
  copyZ(next_b, sizeof(next_b), screen.line_b);
  const bool changed = role_source_ui_ != screen.source || alarm_ch_ != screen.channel ||
                       alarm_n_ != screen.active || strcmp(alarm_a_, next_a) != 0 ||
                       strcmp(alarm_b_, next_b) != 0 || alarm_hist_n_ != screen.hist_n;
  role_source_ui_ = screen.source;
  alarm_ch_ = screen.channel ? screen.channel : 1;
  alarm_n_ = screen.active;
  copyZ(alarm_a_, sizeof(alarm_a_), next_a);
  copyZ(alarm_b_, sizeof(alarm_b_), next_b);
  alarm_hist_n_ = screen.hist_n > 3 ? 3 : screen.hist_n;
  for (uint8_t i = 0; i < 3; ++i) {
    copyZ(alarm_hist_[i], sizeof(alarm_hist_[i]), i < alarm_hist_n_ ? screen.hist[i] : "");
  }
  if (changed && begun_ && !asleep_ &&
      (alarm_locked_ || screen_ == Screen::Alarm || screen_ == Screen::History ||
       screen_ == Screen::Menu)) {
    render();
  }
}

void AccessDisplay::setPcnStatus(const char* line, const char* node, uint16_t alarm_gen) {
  const char* next = line && line[0] ? line : "НОРМА";
  const bool alarm = strcmp(next, "ТРЕВОГА") == 0;
  if (node && node[0]) copyZ(pcn_node_, sizeof(pcn_node_), node);
  (void)alarm_gen;
  if (alarm) {
    const bool fresh = !alarm_show_ || alarm_gen != alarm_seen_;
    copyZ(pcn_line_, sizeof(pcn_line_), "ТРЕВОГА");
    alarm_show_ = true;
    if (fresh) {
      alarm_seen_ = alarm_gen;
      if (alarm_gen != alarm_acked_gen_) alarm_locked_ = true;
      startAlarmBlink();
    }
    return;
  }
  if (alarm_show_) endAlarmShow();
  else if (strcmp(pcn_line_, "НОРМА") != 0) {
    copyZ(pcn_line_, sizeof(pcn_line_), "НОРМА");
    if (begun_ && !asleep_ && screen_ == Screen::Ready) render();
  }
}

void AccessDisplay::setPendingCount(uint16_t n) {
  if (pending_q_ == n) return;
  pending_q_ = n;
  if (begun_ && !asleep_ && screen_ == Screen::Ready) render();
}

void AccessDisplay::setRs485Status(const char* line, bool wake) {
  copyZ(rs485_line_, sizeof(rs485_line_), line && line[0] ? line : "RS485 idle");
  if (wake) {
    wakePanel();
    if (screen_ == Screen::Ready && begun_ && !asleep_) render();
    return;
  }
  if (begun_ && !asleep_ && screen_ == Screen::Ready) render();
}

void AccessDisplay::onShortPress() {
  noteActivity();
  demo_at_ = millis();
  if (alarm_locked_) {
    const uint32_t now = millis();
    if (now - click_at_ > 450) click_n_ = 0;
    click_n_++;
    click_at_ = now;
    if (click_n_ >= 3) {
      click_n_ = 0;
      alarm_locked_ = false;
      alarm_acked_gen_ = alarm_seen_;
      render();
    }
    return;
  }
  if (screen_ == Screen::Bridge) return;
  if (screen_ == Screen::Menu) {
    menu_sel_ = (uint8_t)((menu_sel_ + 1) % (uint8_t)MenuItem::Count);
    render();
    return;
  }
  advanceCycle();
}

void AccessDisplay::onLongPress() {
  noteActivity();
  demo_at_ = millis();
  if (alarm_locked_) return;
  if (screen_ == Screen::Bridge) {
    if (bridge_abort_) bridge_abort_(bridge_abort_ctx_);
    bridge_active_ = false;
    screen_ = Screen::Ready;
    transient_ = false;
    wakePanel();
    if (begun_) render();
    return;
  }
  if (screen_ == Screen::Menu) {
    activateMenuItem();
    return;
  }
  if (screen_ == Screen::Info) {
    screen_ = Screen::Ready;
    transient_ = false;
    render();
    return;
  }
  enterMenu();
}

void AccessDisplay::enterMenu() {
  screen_ = Screen::Menu;
  menu_sel_ = (uint8_t)MenuItem::Info;
  transient_ = false;
  if (radio_) repeater_ = radio_->repeaterEnabled();
  render();
}

void AccessDisplay::activateMenuItem() {
  switch ((MenuItem)menu_sel_) {
    case MenuItem::Info:
      screen_ = Screen::Info;
      transient_ = false;
      render();
      break;
    case MenuItem::Repeater:
      toggleRepeater();
      render();
      break;
    case MenuItem::Alarm:
      screen_ = Screen::Alarm;
      transient_ = false;
      wakePanel();
      render();
      break;
    case MenuItem::Role:
      if (role_toggle_) role_toggle_(role_ctx_);
      if (role_is_) role_source_ui_ = role_is_(role_ctx_);
      render();
      break;
    case MenuItem::Channel:
      if (channel_bump_) channel_bump_(channel_ctx_);
      render();
      break;
    case MenuItem::History:
      screen_ = Screen::History;
      transient_ = false;
      wakePanel();
      render();
      break;
    case MenuItem::Back:
    default:
      screen_ = Screen::Ready;
      transient_ = false;
      render();
      break;
  }
}

void AccessDisplay::toggleRepeater() {
  const bool next = !repeater_;
  if (radio_) {
    radio_->setRepeaterEnabled(next);
    repeater_ = radio_->repeaterEnabled();
  } else {
    repeater_ = next;
  }
}

void AccessDisplay::cycleDemo() {
  static const Screen kCycle[] = {Screen::Granted, Screen::Denied, Screen::Remote,
                                  Screen::Locked, Screen::Emergency, Screen::Ready};
  demo_step_ = (uint8_t)((demo_step_ + 1) % 6);
  screen_ = kCycle[demo_step_];
  transient_ = (screen_ == Screen::Granted || screen_ == Screen::Denied ||
                screen_ == Screen::Remote);
  shown_at_ = millis();

  switch (screen_) {
    case Screen::Granted:
      copyZ(line2_, sizeof(line2_), "Ivan Petrov");
      copyZ(line3_, sizeof(line3_), "ENTRY  12:31:42");
      break;
    case Screen::Denied:
      copyZ(line2_, sizeof(line2_), "НЕИЗВ.КАРТА");
      copyZ(line3_, sizeof(line3_), "12:32:04");
      break;
    case Screen::Remote:
      copyZ(line2_, sizeof(line2_), "ADMIN");
      copyZ(line3_, sizeof(line3_), "ВЫПОЛН...");
      break;
    case Screen::Emergency:
      mesh_ = MeshIndicator::Error;
      break;
    case Screen::Ready:
      if (radio_) {
        mesh_ = radio_->indicator();
        repeater_ = radio_->repeaterEnabled();
      }
      break;
    default:
      if (radio_ && screen_ != Screen::Emergency) {
        mesh_ = radio_->indicator();
      }
      break;
  }
  render();
}

void AccessDisplay::showReady(const char* device_id, MeshIndicator mesh, uint16_t users) {
  copyZ(device_id_, sizeof(device_id_), device_id);
  mesh_ = mesh;
  users_ = users;
  bridge_active_ = false;
  if (screen_ == Screen::Bridge) {
    screen_ = Screen::Alarm;
    transient_ = false;
  }
  OLED_LOG("OLED READY %s mesh=%d users=%u\n", device_id_, (int)mesh_, users_);
  if (begun_ && !asleep_ && !alarm_locked_) render();
}

void AccessDisplay::showGranted(const char* name, const char* time) {
  showGranted(name, "ENTRY", time);
}

void AccessDisplay::showGranted(const char* name, const char* direction, const char* time) {
  copyZ(line2_, sizeof(line2_), name);
  copyZ(last_uid_, sizeof(last_uid_), name ? name : "--");
  char tline[24];
  snprintf(tline, sizeof(tline), "%s  %s", direction && direction[0] ? direction : "ENTRY",
           time ? time : "--:--:--");
  copyZ(line3_, sizeof(line3_), tline);
  screen_ = Screen::Granted;
  transient_ = true;
  shown_at_ = millis();
  OLED_LOG("OLED GRANT %s %s\n", line2_, line3_);
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::showDenied(const char* reason, const char* time) {
  copyZ(line2_, sizeof(line2_), reason);
  copyZ(line3_, sizeof(line3_), time);
  screen_ = Screen::Denied;
  transient_ = true;
  shown_at_ = millis();
  OLED_LOG("OLED DENY %s %s\n", line2_, line3_);
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::showRemote(const char* cmd, const char* operator_id) {
  copyZ(line2_, sizeof(line2_), operator_id);
  copyZ(line3_, sizeof(line3_), cmd && cmd[0] ? cmd : "ВЫПОЛН...");
  screen_ = Screen::Remote;
  transient_ = true;
  shown_at_ = millis();
  OLED_LOG("OLED REMOTE %s %s\n", line2_, line3_);
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::showLocked() {
  screen_ = Screen::Locked;
  transient_ = false;
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::showEmergency() {
  mesh_ = MeshIndicator::Error;
  screen_ = Screen::Emergency;
  transient_ = false;
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::showMeshDiagnostics(const MeshDiagnostics& d) {
  mesh_ = d.indicator;
  repeater_ = d.repeater_enabled;
  OLED_LOG("OLED MESH ok=%d rpt=%d\n", d.radio_ok, d.repeater_enabled);
  if (begun_ && !asleep_) render();
}

void AccessDisplay::setBridgeAbortHandler(void (*fn)(void*), void* ctx) {
  bridge_abort_ = fn;
  bridge_abort_ctx_ = ctx;
}

void AccessDisplay::showBridge(const char* level) {
  copyZ(bridge_level_, sizeof(bridge_level_), level && level[0] ? level : "READ_ONLY");
  bridge_active_ = true;
  screen_ = Screen::Bridge;
  transient_ = false;
  if (strcmp(bridge_level_, "RAW_BRIDGE") == 0) {
    copyZ(rs485_line_, sizeof(rs485_line_), "BR RAW");
  } else if (strcmp(bridge_level_, "CONTROL") == 0) {
    copyZ(rs485_line_, sizeof(rs485_line_), "BR CTL");
  } else {
    copyZ(rs485_line_, sizeof(rs485_line_), "BR RO");
  }
  OLED_LOG("OLED BRIDGE %s\n", bridge_level_);
  wakePanel();
  if (begun_) render();
}

void AccessDisplay::tickChrome() {
#ifndef ACCESS_DISPLAY_SERIAL_ONLY
  if (!begun_ || asleep_ || !panel_ok_) return;
  if ((uint32_t)(millis() - chrome_at_) < 80) return;
  chrome_at_ = millis();
  render();
#endif
}

int AccessDisplay::cycleIndex() const {
  static const Screen kCycle[] = {
      Screen::Alarm,    Screen::Ready, Screen::Granted, Screen::Denied,   Screen::Remote,
      Screen::Locked,   Screen::Emergency, Screen::Info, Screen::History};
  for (int i = 0; i < 9; ++i)
    if (kCycle[i] == screen_) return i;
  return -1;
}

void AccessDisplay::advanceCycle() {
  static const Screen kCycle[] = {
      Screen::Alarm,    Screen::Ready, Screen::Granted, Screen::Denied,   Screen::Remote,
      Screen::Locked,   Screen::Emergency, Screen::Info, Screen::History};
  int i = cycleIndex();
  i = (i < 0) ? 0 : (i + 1) % 9;
  screen_ = kCycle[i];
  transient_ = false;
  switch (screen_) {
    case Screen::Granted:
      copyZ(line2_, sizeof(line2_), "Ivan Petrov");
      copyZ(line3_, sizeof(line3_), "ENTRY  12:31:42");
      break;
    case Screen::Denied:
      copyZ(line2_, sizeof(line2_), "НЕИЗВ.КАРТА");
      copyZ(line3_, sizeof(line3_), "12:32:04");
      break;
    case Screen::Remote:
      copyZ(line2_, sizeof(line2_), "ADMIN");
      copyZ(line3_, sizeof(line3_), "ВЫПОЛН...");
      break;
    case Screen::Emergency:
      mesh_ = MeshIndicator::Error;
      break;
    case Screen::Ready:
      if (radio_) {
        mesh_ = radio_->indicator();
        repeater_ = radio_->repeaterEnabled();
      }
      break;
    default:
      break;
  }
  render();
}

void AccessDisplay::drawBattery() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  return;
#else
  static uint32_t sampled = 0;
  if (millis() > 2500 && (sampled == 0 || (uint32_t)(millis() - sampled) > 8000)) {
    sampled = millis();
#ifndef PIN_VBAT_READ
#define PIN_VBAT_READ 1
#endif
#ifndef PIN_ADC_CTRL
#define PIN_ADC_CTRL 37
#endif
#ifndef ADC_MULTIPLIER
#define ADC_MULTIPLIER 5.42f
#endif
    analogReadResolution(10);
    pinMode(PIN_ADC_CTRL, OUTPUT);
    const int idle = digitalRead(PIN_ADC_CTRL);
#ifdef HELTEC_LORA_V3
    digitalWrite(PIN_ADC_CTRL, LOW);
#else
    digitalWrite(PIN_ADC_CTRL, HIGH);
#endif
    delay(2);
    uint32_t raw = 0;
    for (int i = 0; i < 4; ++i) raw += analogRead(PIN_VBAT_READ);
    raw /= 4;
#ifdef HELTEC_LORA_V3
    if (raw < 80) {
      digitalWrite(PIN_ADC_CTRL, HIGH);
      delay(2);
      raw = 0;
      for (int i = 0; i < 4; ++i) raw += analogRead(PIN_VBAT_READ);
      raw /= 4;
    }
#endif
    digitalWrite(PIN_ADC_CTRL, idle);
    const float mv = (float)ADC_MULTIPLIER * (3.3f / 1024.0f) * (float)raw * 1000.0f;
    int pct = (int)((mv - 3300.0f) * 100.0f / 900.0f);
    if (mv < 2800.0f || mv > 4800.0f) pct = 0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    batt_pct_ = (uint8_t)pct;
  }
  const int16_t x = 108;
  const int16_t y = 0;
  g_oled.drawRect(x, y, 16, 8, SSD1306_WHITE);
  g_oled.fillRect(x + 16, y + 2, 2, 4, SSD1306_WHITE);
  const int fill = (int)batt_pct_ * 12 / 100;
  if (fill > 0) g_oled.fillRect(x + 2, y + 2, fill, 4, SSD1306_WHITE);
#endif
}

void AccessDisplay::drawSweep() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  return;
#else
  const int span = 148;
  int pos = (int)((millis() / 16) % (uint32_t)span) - 20;
  int w = 20;
  if (pos < 0) {
    w += pos;
    pos = 0;
  }
  if (pos + w > 128) w = 128 - pos;
  if (w > 0) g_oled.fillRect(pos, 62, w, 2, SSD1306_WHITE);
#endif
}

void AccessDisplay::drawScreenNo() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  return;
#else
  const int idx = cycleIndex();
  if (idx < 0 || alarm_locked_) return;
  g_oled.setTextSize(1);
  g_oled.setTextColor(SSD1306_WHITE);
  g_oled.setCursor(122, 54);
  g_oled.print(idx + 1);
#endif
}

#ifndef ACCESS_DISPLAY_SERIAL_ONLY
static void drawBlinkLine(int16_t y, const char* text, bool on) {
  if (!text || !text[0] || !on) return;
  g_oled.fillRect(0, y, 128, 8, SSD1306_WHITE);
  oledPrintUtf8(g_oled, 0, y, text, true);
}
#endif

void AccessDisplay::drawAlarmOverlay() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  Serial.println("OLED ALARM lock");
#else
  const bool phase = ((millis() / 250u) & 1u) == 0;
  oledPrintUtf8(g_oled, 0, 0, "ТРЕВОГА");
  char head[28];
  snprintf(head, sizeof(head), "К%02u А%u", (unsigned)alarm_ch_, (unsigned)alarm_n_);
  oledPrintUtf8(g_oled, 0, 12, head);
  // Only a live alarm row blinks: black glyphs on a white bar.
  drawBlinkLine(26, alarm_a_, phase);
  if (!role_source_ui_) drawBlinkLine(42, alarm_b_, phase);
  drawBattery();
  drawSweep();
#endif
}

void AccessDisplay::drawMeshIndicator() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  (void)0;
#else
  // Spec: M● / M○ / M! top-right, optional R if repeater on.
  // Leave x=108..127 for the battery icon.
  int16_t x = 108;
  if (repeater_) x -= 6;
  if (mesh_ == MeshIndicator::Error) {
    x -= 12;
    g_oled.setCursor(x, 0);
    g_oled.print("M!");
  } else {
    x -= 14;
    g_oled.setCursor(x, 0);
    g_oled.print("M");
    const int16_t cx = x + 10;
    const int16_t cy = 3;
    if (mesh_ == MeshIndicator::Active) {
      g_oled.fillCircle(cx, cy, 2, SSD1306_WHITE);
    } else {
      g_oled.drawCircle(cx, cy, 2, SSD1306_WHITE);
    }
  }
  if (repeater_) {
    g_oled.setCursor(98, 0);
    g_oled.print("R");
  }
#endif
}

void AccessDisplay::render() {
#ifdef ACCESS_DISPLAY_SERIAL_ONLY
  Serial.printf("OLED screen=%d %s rpt=%d\n", (int)screen_, device_id_, (int)repeater_);
#else
  if (!panel_ok_ || asleep_) return;

  g_oled.clearDisplay();
  g_oled.setTextSize(1);
  g_oled.setTextColor(SSD1306_WHITE);

  // The alarm notice stays above every screen, including the menu.
  if (alarm_show_) {
    drawAlarmOverlay();
    g_oled.display();
    return;
  }

  switch (screen_) {
    case Screen::Ready: {
      oledPrintUtf8(g_oled, 0, 0, device_id_);
      drawMeshIndicator();
      if (pending_q_ > 0) {
        char ready[20];
        snprintf(ready, sizeof(ready), "ГОТОВ Q:%u", (unsigned)pending_q_);
        oledPrintUtf8(g_oled, 0, 16, ready);
      } else {
        oledPrintUtf8(g_oled, 0, 16, "ГОТОВ");
      }
      oledPrintUtf8(g_oled, 0, 32, "ПРИЛОЖИТЕ КАРТУ");
      oledPrintUtf8(g_oled, 0, 40, pcn_line_);
      oledPrintUtf8(g_oled, 0, 48, pp_line_);
      oledPrintUtf8(g_oled, 0, 56, rs485_line_);
      break;
    }

    case Screen::Granted:
      oledPrintUtf8(g_oled, 0, 0, "ДОСТУП ОК");
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 20, line2_);
      oledPrintUtf8(g_oled, 0, 36, line3_);
      break;

    case Screen::Denied:
      oledPrintUtf8(g_oled, 0, 0, "ОТКАЗ");
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 20, line2_);
      oledPrintUtf8(g_oled, 0, 36, line3_);
      break;

    case Screen::Remote:
      oledPrintUtf8(g_oled, 0, 0, "ДИСТАНЦ.");
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 20, line2_);
      oledPrintUtf8(g_oled, 0, 36, line3_);
      break;

    case Screen::Locked:
      oledPrintUtf8(g_oled, 0, 0, "БЛОКИРОВКА");
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 24, "СТОП");
      break;

    case Screen::Emergency:
      oledPrintUtf8(g_oled, 0, 0, "АВАРИЯ");
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 24, "СТОП");
      break;

    case Screen::Menu: {
      oledPrintUtf8(g_oled, 0, 0, "МЕНЮ");
      const bool src = role_is_ ? role_is_(role_ctx_) : role_source_ui_;
      char item[28];
      const char* labels[7];
      char rpt[28];
      char role[28];
      char chan[28];
      const uint8_t ch = channel_get_ ? channel_get_(channel_ctx_) : alarm_ch_;
      snprintf(rpt, sizeof(rpt), "ПОВТОР %s", repeater_ ? "ВКЛ" : "ВЫКЛ");
      snprintf(role, sizeof(role), "РОЛЬ %s", src ? "ИСТОЧНИК" : "ПРИЕМНИК");
      snprintf(chan, sizeof(chan), "КАНАЛ %02u", (unsigned)ch);
      labels[0] = "ИНФО";
      labels[1] = rpt;
      labels[2] = "СИГНАЛИЗАЦИЯ";
      labels[3] = role;
      labels[4] = chan;
      labels[5] = "ИСТОРИЯ";
      labels[6] = "НАЗАД";
      // Five rows. Keep the selected row on screen by shifting a window of 4.
      uint8_t first = 0;
      if (menu_sel_ > 3) first = (uint8_t)(menu_sel_ - 3);
      for (uint8_t row = 0; row < 4; ++row) {
        const uint8_t idx = (uint8_t)(first + row);
        if (idx >= (uint8_t)MenuItem::Count) break;
        snprintf(item, sizeof(item), "%s %s", idx == menu_sel_ ? ">" : " ", labels[idx]);
        oledPrintUtf8(g_oled, 0, (int16_t)(14 + row * 12), item);
      }
      break;
    }

    case Screen::Alarm: {
      char head[28];
      snprintf(head, sizeof(head), "СИГНАЛ К%02u А%u", (unsigned)alarm_ch_, (unsigned)alarm_n_);
      oledPrintUtf8(g_oled, 0, 0, head);
      const bool blink_on = ((millis() / 250u) & 1u) == 0;
      drawBlinkLine(20, alarm_a_, blink_on);
      drawBlinkLine(36, alarm_b_, blink_on);
      break;
    }

    case Screen::History:
      oledPrintUtf8(g_oled, 0, 0, "ИСТОРИЯ");
      if (alarm_hist_n_ == 0) {
        oledPrintUtf8(g_oled, 0, 20, "пусто");
      } else {
        for (uint8_t i = 0; i < alarm_hist_n_ && i < 3; ++i) {
          oledPrintUtf8(g_oled, 0, (int16_t)(16 + i * 14), alarm_hist_[i]);
        }
      }
      break;

    case Screen::Bridge:
      oledPrintUtf8(g_oled, 0, 0, device_id_);
      drawMeshIndicator();
      oledPrintUtf8(g_oled, 0, 16, "МОСТ");
      oledPrintUtf8(g_oled, 0, 32, bridge_level_);
      oledPrintUtf8(g_oled, 0, 48, "USER стоп");
      break;

    case Screen::Info: {
      const uint32_t sec = (millis() - boot_ms_) / 1000;
      oledPrintUtf8(g_oled, 0, 0, "ИНФО");
      drawMeshIndicator();
      char idl[24];
      snprintf(idl, sizeof(idl), "id %s", device_id_);
      oledPrintUtf8(g_oled, 0, 16, idl);
      char users[20];
      snprintf(users, sizeof(users), "users %u", (unsigned)users_);
      oledPrintUtf8(g_oled, 0, 28, users);
      char last[28];
      snprintf(last, sizeof(last), "last %s", last_uid_);
      oledPrintUtf8(g_oled, 0, 40, last);
      char heap[28];
      snprintf(heap, sizeof(heap), "heap %u up %us", (unsigned)ESP.getFreeHeap(),
               (unsigned)sec);
      oledPrintUtf8(g_oled, 0, 52, heap);
      break;
    }
    default:
      break;
  }

  drawBattery();
  drawSweep();
  drawScreenNo();
  g_oled.display();
#endif
}
