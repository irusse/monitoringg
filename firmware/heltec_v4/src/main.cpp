#include <Arduino.h>
#include <stdio.h>
#ifndef ACCESS_STUB_NO_RADIO
#include <BLEDevice.h>
#include <SPI.h>
#include <SPIFFS.h>
#endif
#include "access/pins.h"
#include "access/access_controller.h"
#include "reader/wiegand_reader.h"
#include "turnstile/turnstile_hal.h"
#include "meshcore_adapter/meshcore_service.h"
#include "display/access_display.h"
#include "hal/rs485.h"
#include "mesh485/dispatcher.h"
#include "mesh485/hal_port.h"

#ifndef ACCESS_STUB_NO_RADIO
SET_LOOP_TASK_STACK_SIZE(24576);
#endif

static MeshCoreService mesh;
static WiegandReader reader;
static TurnstileHAL turnstile;
static AccessDisplay oled;
static Rs485Hal rs485;
static Rs485HalPort rs485_port(rs485);
static Mesh485Dispatcher mesh485;

static void hwOpenPulse(uint16_t ms, void*) { turnstile.openPulse(ms); }
static void hwLock(void*) { turnstile.lock(); }
static void hwShowRemote(const char* cmd, const char* optr, void*) {
  oled.showRemote(cmd, optr);
}

static uint16_t aclUsers() { return mesh485.acl().enabledCount(); }

static bool roleIsSource(void*) { return mesh485.roleSource(); }
static void roleToggle(void*) { mesh485.toggleRole(); }
static uint8_t channelGet(void*) { return mesh485.channel(); }
static void channelBump(void*) { mesh485.bumpChannel(); }

static size_t onMesh485Json(const char* line, char* out, size_t out_max, void*) {
  Mesh485Env env;
  env.port = &rs485_port;
  const MeshDiagnostics d = mesh.diagnostics();
  env.radio_ok = d.radio_ok;
  env.repeater = d.repeater_enabled;
  env.rssi = d.rssi;
  env.snr = d.snr;
  env.mesh_rx = d.rx;
  env.mesh_tx = d.tx;
  env.uptime_s = millis() / 1000;
  env.heap = ESP.getFreeHeap();
  env.unix_now = 0;
  env.node_id = MESH485_NODE_ID;
  env.source = "USB";
  return mesh485.handleLine(line, env, out, out_max);
}

static void onBridgeAbort(void*) { mesh485.closeSession(); }

static MeshIndicator last_mesh = MeshIndicator::Inactive;
static bool last_repeater = false;
static TurnstileState last_gate = TurnstileState::Locked;
static bool last_bridge = false;

static void nowHms(char* buf, size_t n) {
  const uint32_t s = millis() / 1000;
  snprintf(buf, n, "%02u:%02u:%02u", (unsigned)((s / 3600) % 24),
           (unsigned)((s / 60) % 60), (unsigned)(s % 60));
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("3MESH ACCESS");

  mesh.prepareBoard();
  oled.attachRadio(&mesh);
  oled.begin();
  oled.setBridgeAbortHandler(onBridgeAbort, nullptr);
  oled.setRoleHandlers(roleIsSource, roleToggle, nullptr);
  oled.setChannelHandlers(channelGet, channelBump, nullptr);
  mesh485.beginStore();
  oled.showReady("TS-001", MeshIndicator::Inactive, aclUsers());

  rs485.begin();
  oled.setRs485Status(rs485.ok() ? "M485 9600" : "RS485 fail");

  mesh.setMesh485Handler(onMesh485Json, nullptr);
  if (!mesh.start()) {
    MeshDiagnostics d = mesh.diagnostics();
    oled.showMeshDiagnostics(d);
  } else {
    last_mesh = mesh.indicator();
    last_repeater = mesh.repeaterEnabled();
    oled.showReady("TS-001", last_mesh, aclUsers());
    oled.showMeshDiagnostics(mesh.diagnostics());
  }

  // RTC/sensor I2C scan shares Wire with the OLED — restore the panel.
  oled.reinit();
  oled.showReady("TS-001", mesh.indicator(), aclUsers());
  oled.setRs485Status(rs485.ok() ? "M485 9600" : "RS485 fail");

#ifdef MESH485_RDM6300
  mesh485.setRdmMode(true);
  rs485.setDirection(false);
#elif !defined(HELTEC_LORA_V3)
  reader.begin(PIN_WIEGAND_D0, PIN_WIEGAND_D1);
#endif
  turnstile.begin();
  {
    AccessHwHooks hw;
    hw.openPulse = hwOpenPulse;
    hw.lock = hwLock;
    hw.showRemote = hwShowRemote;
    mesh485.bindAccessHw(hw);
  }
  mesh485.devices().bindPort(&rs485_port);
  mesh485.devices().beginAll();
  last_gate = turnstile.state();
}

void loop() {
  mesh.loop();
  turnstile.loop();
  rs485.loop();
  mesh485.devices().bindPort(&rs485_port);
  mesh485.pollDevices();
  mesh485.tick(millis() / 1000);
  {
    mesh485.setAlarmName(mesh.nodeName());
    uint8_t hop[96];
    const size_t hn = mesh485.pollPcnHop(hop, sizeof(hop), millis());
    if (hn) {
#ifdef MESH485_DEBUG
      Serial.printf("PCN hop TX %u bytes\n", (unsigned)hn);
#endif
      mesh.send(hop, hn);
    }
    uint8_t rx[96];
    size_t rn = 0;
    if (mesh.poll(rx, &rn, sizeof(rx)) && rn) {
      uint8_t ack[80];
      size_t an = 0;
      if (mesh485.acceptPcnFrame(rx, rn, ack, &an) && an) mesh.send(ack, an);
    }
  }
  {
    uint32_t seq = 0;
    static char evline[MESH485_RESP_MAX];
    const size_t en = mesh485.nextPendingEventLine(evline, sizeof(evline), &seq);
    if (en) {
      if (mesh.writeMesh485Line(evline, en) == en) {
        mesh485.ackMesh(seq);
      } else {
        mesh485.failMesh(seq);
      }
    }
  }
  oled.setPendingCount((uint16_t)mesh485.events().pendingCount());
  oled.setUsers(aclUsers());
  {
    char pp[20];
    if (strcmp(mesh485.devices().busProtocol(), "BOLID_SIGNAL10") == 0) {
      snprintf(pp, sizeof(pp), "S10-001 %s", mesh485.devices().signal10().overlayState());
    } else if (strcmp(mesh485.devices().busProtocol(), "BOLID_SIGNAL20") == 0) {
      snprintf(pp, sizeof(pp), "S20-001 %s", mesh485.devices().signal20().overlayState());
    } else {
      snprintf(pp, sizeof(pp), "PP-001 %s", mesh485.devices().c2000pp().overlayState());
    }
    oled.setPpStatus(pp);
  }
  {
    PcnScreen alarm;
    mesh485.fillAlarm(&alarm);
    oled.setAlarmText(alarm);
    // A source blinks only for its own contact. Another source on the same
    // channel must not light this panel.
    const bool source = mesh485.roleSource();
    const char* shown = source ? mesh485.devices().pcn().overlayState() : mesh485.pcnShownState();
    if (!shown || !shown[0]) shown = "NORMAL";
    oled.setPcnStatus(strcmp(shown, "ALARM") == 0 ? "ТРЕВОГА" : "НОРМА",
                      source ? mesh485.nodeId() : mesh485.pcnFromNode(), mesh485.pcnAlarmGen());
  }
  oled.loop();

  const bool br = mesh485.sessionOpen();
  if (br != last_bridge) {
    last_bridge = br;
    if (br) {
      oled.showBridge(mesh485.sessionLevelName());
    } else {
      oled.showReady("TS-001", mesh.indicator(), aclUsers());
      oled.setRs485Status(rs485.ok() ? "M485 9600" : "RS485 fail", true);
    }
  }

  uint8_t buf[128];
  const size_t got = rs485.read(buf, sizeof(buf));
#ifdef MESH485_RDM6300
  if (got) {
    const bool emergency =
        turnstile.state() == TurnstileState::Emergency || turnstile.emergencyAsserted();
    bool opened = false;
    const char* dir = turnstile.exitAsserted() ? "EXIT" : "ENTRY";
    if (mesh485.pushRdm(buf, got, millis(), dir, !emergency, &opened)) {
      char tbuf[12];
      nowHms(tbuf, sizeof(tbuf));
      if (emergency) {
        oled.showDenied("АВАРИЯ", tbuf);
      } else if (opened) {
        const char* who = mesh485.acl().lastUid();
        oled.showGranted(who && who[0] ? who : "КАРТА", dir, tbuf);
      } else {
        oled.showDenied("НЕИЗВ.КАРТА", tbuf);
        turnstile.lampGreen(false);
        turnstile.lampRed(true);
      }
    }
  }
#else
  if (got) {
    if (mesh485.sessionOpen()) {
      mesh485.captureRx(buf, got, (int64_t)(millis() / 1000));
    } else {
      char line[20];
      snprintf(line, sizeof(line), "RS485 %u B", (unsigned)got);
      oled.setRs485Status(line, true);
      mesh485.devices().raw().noteIdleRx(got);
    }
  }

  CardEvent ev;
  if (reader.poll(ev)) {
    const bool emergency =
        turnstile.state() == TurnstileState::Emergency || turnstile.emergencyAsserted();
    bool opened = false;
    const char* dir = turnstile.exitAsserted() ? "EXIT" : "ENTRY";
    mesh485.acceptCard(ev.uid, (int64_t)(millis() / 1000), dir, !emergency, &opened);
    if (emergency) {
      char tbuf[12];
      nowHms(tbuf, sizeof(tbuf));
      oled.showDenied("АВАРИЯ", tbuf);
    } else if (opened) {
      char tbuf[12];
      nowHms(tbuf, sizeof(tbuf));
      const AccessDecision d = mesh485.acl().checkUid(ev.uid);
      oled.showGranted(d.name[0] ? d.name : ev.uid, dir, tbuf);
    } else {
      char tbuf[12];
      nowHms(tbuf, sizeof(tbuf));
      oled.showDenied("НЕИЗВ.КАРТА", tbuf);
      turnstile.lampGreen(false);
      turnstile.lampRed(true);
    }
  }
#endif

  const TurnstileState st = turnstile.state();
  if (st == TurnstileState::Emergency) {
    mesh485.devices().access().setGateState("EMERGENCY");
  } else if (st == TurnstileState::Open || st == TurnstileState::Opening) {
    mesh485.devices().access().setGateState("OPEN");
  } else {
    mesh485.devices().access().setGateState("LOCKED");
  }
  if (st != last_gate) {
    last_gate = st;
    if (st == TurnstileState::Emergency) {
      oled.showEmergency();
    } else if (st == TurnstileState::Locked) {
      if (mesh485.sessionOpen()) {
        oled.showBridge(mesh485.sessionLevelName());
      } else {
        oled.showReady("TS-001", mesh.indicator(), aclUsers());
      }
    }
  }

  MeshDiagnostics d = mesh.diagnostics();
  MeshIndicator ind = mesh.indicator();
  if (ind != last_mesh || d.repeater_enabled != last_repeater) {
    last_mesh = ind;
    last_repeater = d.repeater_enabled;
    oled.showMeshDiagnostics(d);
  }
}
