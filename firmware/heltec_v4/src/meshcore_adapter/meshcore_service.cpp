#include "meshcore_adapter/meshcore_service.h"

#ifdef ACCESS_STUB_NO_RADIO

void MeshCoreService::prepareBoard() { board_ready_ = true; }

bool MeshCoreService::start() {
  running_ = true;
  radio_ok_ = false;
  repeater_ = true;
  return true;
}

void MeshCoreService::stop() { running_ = false; }

void MeshCoreService::loop() {}

MeshIndicator MeshCoreService::indicator() const {
  if (!running_) return MeshIndicator::Inactive;
  return MeshIndicator::Error;
}

MeshDiagnostics MeshCoreService::diagnostics() const {
  MeshDiagnostics d;
  d.indicator = MeshIndicator::Error;
  d.radio_ok = false;
  d.repeater_enabled = repeater_;
  return d;
}

void MeshCoreService::setRepeaterEnabled(bool on) { repeater_ = on; }

bool MeshCoreService::repeaterEnabled() const { return repeater_; }

void MeshCoreService::setMesh485Handler(Mesh485JsonFn, void*) {}

size_t MeshCoreService::writeMesh485Line(const char*, size_t) { return 0; }

const char* MeshCoreService::nodeName() const { return ""; }

bool MeshCoreService::send(const uint8_t*, size_t) { return false; }

bool MeshCoreService::poll(uint8_t*, size_t* len, size_t) {
  if (len) *len = 0;
  return false;
}

#else

#include <Arduino.h>
#include <SPIFFS.h>
#include <string.h>
#include <Mesh.h>
#include <helpers/MultiSerialInterface.h>
#include <helpers/ArduinoSerialInterface.h>
#include <helpers/esp32/SerialBLEInterface.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/SimpleMeshTables.h>
#include <target.h>
#define the_mesh the_mesh_extern_unused
#include "MyMesh.h"
#undef the_mesh
#include "mesh485/usb_mux.h"
#include "mesh485/ble_mux.h"
#include "mesh485/pcn_hop.h"

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "3MESH-"
#endif

#ifndef ADVERT_NAME
#define ADVERT_NAME "TS-001"
#endif

static MultiSerialInterface interface_manager;
static SerialBLEInterface bluetooth_interface;
static BleMuxInterface ble_mux;
static ArduinoSerialInterface usb_serial_interface;
static UsbMuxStream usb_mux;

static DataStore store(SPIFFS, rtc_clock);
static StdRNG fast_rng;
static SimpleMeshTables tables;

static uint8_t g_hop_rx[4][MAX_PACKET_PAYLOAD];
static uint8_t g_hop_rx_len[4];
static uint8_t g_hop_rx_r = 0;
static uint8_t g_hop_rx_w = 0;
static uint8_t g_hop_rx_n = 0;

static void hop_rx_push(const uint8_t* data, size_t len) {
  if (!data || len == 0 || len > MAX_PACKET_PAYLOAD || g_hop_rx_n >= 4) return;
  memcpy(g_hop_rx[g_hop_rx_w], data, len);
  g_hop_rx_len[g_hop_rx_w] = (uint8_t)len;
  g_hop_rx_w = (uint8_t)((g_hop_rx_w + 1) % 4);
  ++g_hop_rx_n;
}

// Companion MyMesh stays in vendor/. This subclass only keeps PCN hop frames
// out of the companion raw-data push (that path is not the product journal).
class AccessMesh : public MyMesh {
public:
  AccessMesh(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc, SimpleMeshTables& tables,
             DataStore& store)
      : MyMesh(radio, rng, rtc, tables, store, NULL) {}

  void onRawDataRecv(mesh::Packet* packet) override {
    if (packet && pcn_hop_is(packet->payload, packet->payload_len)) {
      hop_rx_push(packet->payload, packet->payload_len);
    }
  }

  // Only our alarm frame. Public chat and other group data are ignored.
  void onChannelDataRecv(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint16_t data_type,
                         const uint8_t* data, size_t data_len) override {
    (void)channel;
    (void)pkt;
    if (data_type == 0xA501 && data && pcn_hop_is(data, data_len)) {
      hop_rx_push(data, data_len);
    }
  }

  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override {}
  void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*,
                           const char*) override {}
  void onAdvertRecv(mesh::Packet*, const mesh::Identity&, uint32_t, const uint8_t*, size_t) override {}
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override {}
  uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override {
    return 0;
  }

  bool allowPacketForward(const mesh::Packet*) override { return false; }

  // Do not hold city floods in the packet pool. A full pool is what stopped
  // the V4 from queueing its own alarm.
  int calcRxDelay(float, uint32_t) const override { return 0; }
  int freePackets() const { return _mgr ? _mgr->getFreeCount() : -1; }
};

AccessMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store);

void MeshCoreService::prepareBoard() {
  if (board_ready_) return;
  board.begin();
  board_ready_ = true;
}

bool MeshCoreService::start() {
  prepareBoard();

  if (!radio_init()) {
    radio_ok_ = false;
    running_ = false;
    Serial.println("RADIO init failed");
    return false;
  }
  radio_ok_ = true;

  fast_rng.begin(radio_driver.getRngSeed());

  SPIFFS.begin(true);
  store.begin();

  const bool first_access = !SPIFFS.exists("/3mesh_access");
  the_mesh.begin(false);

  if (first_access) {
    File marker = SPIFFS.open("/3mesh_access", "w");
    if (marker) {
      marker.print("1");
      marker.close();
    }
  }
  // These nodes do not repeat city traffic. Their own alarm is still a flood,
  // so a city repeater can carry it.
  if (the_mesh.getNodePrefs()->isRepeatEn() || the_mesh.getNodePrefs()->rx_delay_base != 0) {
    the_mesh.getNodePrefs()->setRepeatEn(false);
    the_mesh.getNodePrefs()->rx_delay_base = 0;
    the_mesh.savePrefs();
  }
  repeater_ = false;

  bluetooth_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name,
                            the_mesh.getBLEPin());
  ble_mux.bind(bluetooth_interface);
  interface_manager.addInterface(InterfaceType::Bluetooth, &ble_mux);

  snprintf(ble_name_, sizeof(ble_name_), "%s%s", BLE_NAME_PREFIX,
           the_mesh.getNodePrefs()->node_name);
  ble_pin_ = the_mesh.getBLEPin();

  Serial.printf("BLE name: %s\n", ble_name_);
  Serial.printf("BLE pin: %u\n", (unsigned)ble_pin_);
  Serial.printf("LoRa %.3f MHz SF%u BW %.2f CR%u node %s name %s\n",
                the_mesh.getNodePrefs()->freq, (unsigned)the_mesh.getNodePrefs()->sf,
                the_mesh.getNodePrefs()->bw, (unsigned)the_mesh.getNodePrefs()->cr,
                MESH485_NODE_ID, the_mesh.getNodePrefs()->node_name);
  Serial.println("USB CDC ready");

  // MESH 485 JSON (first byte '{') is intercepted; Companion frames pass through.
  usb_mux.begin(Serial);
  usb_serial_interface.begin(usb_mux);
  interface_manager.addInterface(InterfaceType::USB, &usb_serial_interface);

  the_mesh.startInterface(interface_manager);
  sensors.begin();
#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif
  board.onBootComplete();

  running_ = true;
  return true;
}

void MeshCoreService::stop() {
  interface_manager.disable();
  running_ = false;
}

void MeshCoreService::loop() {
  if (!running_) return;
  usb_mux.pump();
  the_mesh.loop();
  interface_manager.loop();
  sensors.loop();
  rtc_clock.tick();
}

void MeshCoreService::setMesh485Handler(Mesh485JsonFn fn, void* ctx) {
  usb_mux.setHandler(fn, ctx);
  ble_mux.setHandler(fn, ctx);
}

size_t MeshCoreService::writeMesh485Line(const char* line, size_t n) {
  size_t usb = 0;
  if (line && n) {
    usb = Serial.write((const uint8_t*)line, n);
    if (usb == n) Serial.write('\n');
  }
  return usb;
}

const char* MeshCoreService::nodeName() const {
  const char* name = the_mesh.getNodePrefs()->node_name;
  return name ? name : "";
}

MeshIndicator MeshCoreService::indicator() const {
  if (!radio_ok_) return MeshIndicator::Error;
  if (!running_) return MeshIndicator::Inactive;
  return MeshIndicator::Active;
}

MeshDiagnostics MeshCoreService::diagnostics() const {
  MeshDiagnostics d;
  d.indicator = indicator();
  d.radio_ok = radio_ok_ && running_;
  d.repeater_enabled = repeater_;
  if (radio_ok_) {
    d.rssi = (int)radio_driver.getLastRSSI();
    d.snr = radio_driver.getLastSNR();
    d.rx = radio_driver.getPacketsRecv();
    d.tx = radio_driver.getPacketsSent();
  }
  return d;
}

void MeshCoreService::setRepeaterEnabled(bool) {
  repeater_ = false;
  if (the_mesh.getNodePrefs()->isRepeatEn()) {
    the_mesh.getNodePrefs()->setRepeatEn(false);
    the_mesh.savePrefs();
  }
}

bool MeshCoreService::repeaterEnabled() const { return repeater_; }

bool MeshCoreService::send(const uint8_t* data, size_t len) {
  if (!running_ || !radio_ok_ || !data || len == 0 || len > MAX_PACKET_PAYLOAD) {
    Serial.printf("PCN flood skip run=%d radio=%d len=%u\n", (int)running_, (int)radio_ok_,
                  (unsigned)len);
    return false;
  }
  ChannelDetails channel;
  bool found = false;
  for (int i = 0; i < 8; ++i) {
    ChannelDetails row;
    if (!the_mesh.getChannel(i, row)) break;
    if (row.name[0] && strcmp(row.name, "Public") == 0) {
      channel = row;
      found = true;
      break;
    }
  }
  if (!found && !the_mesh.getChannel(0, channel)) {
    Serial.println("PCN flood no channel");
    return false;
  }
  // Public-channel group flood. Stock repeaters retransmit this; zero-hop RAW_CUSTOM they do not.
  const bool ok =
      the_mesh.sendGroupData(channel.channel, nullptr, OUT_PATH_UNKNOWN, 0xA501, data, (int)len);
  if (ok) Serial.println("PCN flood");
  else Serial.printf("PCN flood send fail free=%d\n", the_mesh.freePackets());
  return ok;
}

bool MeshCoreService::poll(uint8_t* buf, size_t* len, size_t max_len) {
  if (len) *len = 0;
  if (!buf || !len || g_hop_rx_n == 0) return false;
  const uint8_t n = g_hop_rx_len[g_hop_rx_r];
  if (n > max_len) return false;
  memcpy(buf, g_hop_rx[g_hop_rx_r], n);
  *len = n;
  g_hop_rx_r = (uint8_t)((g_hop_rx_r + 1) % 4);
  --g_hop_rx_n;
  return true;
}

#endif
