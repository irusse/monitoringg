#include "access/access_controller.h"
#include "display/alarm_ui.h"
#include "mesh485/auth.h"
#include "mesh485/bus_cfg.h"
#include "mesh485/base64.h"
#include "mesh485/clock.h"
#include "mesh485/codec.h"
#include "mesh485/dispatcher.h"
#include "mesh485/pcn_hop.h"
#include "mesh485/roles.h"
#include "mesh_access/map_protocol.h"
#include "orion_vectors.h"
#include "protocols/bolid/crc8.h"
#include "protocols/bolid/signal10/signal10_adapter.h"
#include "protocols/bolid/signal20/signal20_adapter.h"
#include "protocols/modbus/crc16.h"
#include "storage/event_record.h"
#include "storage/event_store.h"

#include <stdio.h>
#include <string.h>

int g_fail = 0;

int run_orion_crc_tests();

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL: %s\n", msg);                                               \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

static void rdm_frame(uint8_t out[14], const uint8_t raw[5], bool bad_sum) {
  out[0] = 0x02;
  uint8_t x = 0;
  for (int i = 0; i < 5; ++i) {
    x = (uint8_t)(x ^ raw[i]);
    snprintf((char*)out + 1 + i * 2, 3, "%02X", raw[i]);
  }
  if (bad_sum) x = (uint8_t)(x ^ 0xFF);
  snprintf((char*)out + 11, 3, "%02X", x);
  out[13] = 0x03;
}

static int count_substr(const char* hay, const char* needle) {
  int n = 0;
  if (!hay || !needle || !needle[0]) return 0;
  const size_t step = strlen(needle);
  for (const char* p = hay; (p = strstr(p, needle)) != nullptr; p += step) ++n;
  return n;
}

class LoopbackPort : public Mesh485Port {
public:
  size_t writeFrame(const uint8_t* data, size_t len) override {
    if (!data || len == 0) return 0;
    if (len > sizeof(buf_)) len = sizeof(buf_);
    memcpy(buf_, data, len);
    len_ = len;
    tx_ += (uint32_t)len;
    return len;
  }
  size_t readFrame(uint8_t* buf, size_t max, uint32_t) override {
    size_t n = len_ < max ? len_ : max;
    if (buf && n) memcpy(buf, buf_, n);
    rx_ += (uint32_t)n;
    len_ = 0;
    return n;
  }
  uint32_t rxCount() const override { return rx_; }
  uint32_t txCount() const override { return tx_; }
  uint32_t baud() const override { return 9600; }
  char parity() const override { return 'N'; }
  uint8_t stopBits() const override { return 1; }
  bool ok() const override { return true; }
  void setDirection(bool tx) override { de_ = tx; }
  bool txDirection() const override { return de_; }

private:
  uint8_t buf_[256] = {};
  size_t len_ = 0;
  uint32_t rx_ = 0;
  uint32_t tx_ = 0;
  bool de_ = false;
};

class EmptyBusPort : public Mesh485Port {
public:
  size_t writeFrame(const uint8_t* data, size_t len) override {
    (void)data;
    tx_ += (uint32_t)len;
    return len;
  }
  size_t readFrame(uint8_t*, size_t, uint32_t) override { return 0; }
  uint32_t rxCount() const override { return 0; }
  uint32_t txCount() const override { return tx_; }
  uint32_t baud() const override { return 9600; }
  char parity() const override { return 'N'; }
  uint8_t stopBits() const override { return 1; }
  bool ok() const override { return true; }
  void setDirection(bool tx) override { de_ = tx; }
  bool txDirection() const override { return de_; }

private:
  uint32_t tx_ = 0;
  bool de_ = false;
};

// Native fake converter: FC03 @ 46152 qty 2 → type 36 + version, or flipped CRC.
class ModbusReplyPort : public Mesh485Port {
public:
  enum class Mode : uint8_t { Valid = 0, BadCrc = 1, Silent = 2 };
  void setMode(Mode m) { mode_ = m; }

  size_t writeFrame(const uint8_t* data, size_t len) override {
    tx_ += (uint32_t)len;
    pending_len_ = 0;
    if (mode_ == Mode::Silent) return len;
    if (!data || len < 6 || data[0] != 1 || data[1] != 0x03) return len;
    const uint16_t start = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);
    if (start != 46152) return len;
    // 01 03 04 00 24 00 11 + CRC (type 36, version 17). Not synthesized by the adapter.
    uint8_t r[9] = {0x01, 0x03, 0x04, 0x00, 0x24, 0x00, 0x11, 0, 0};
    crc16_modbus_append(r, 7, sizeof(r));
    if (mode_ == Mode::BadCrc) r[8] = (uint8_t)(r[8] ^ 0xFF);
    memcpy(pending_, r, 9);
    pending_len_ = 9;
    return len;
  }
  size_t readFrame(uint8_t* buf, size_t max, uint32_t) override {
    size_t n = pending_len_ < max ? pending_len_ : max;
    if (buf && n) memcpy(buf, pending_, n);
    rx_ += (uint32_t)n;
    pending_len_ = 0;
    return n;
  }
  uint32_t rxCount() const override { return rx_; }
  uint32_t txCount() const override { return tx_; }
  uint32_t baud() const override { return 9600; }
  char parity() const override { return 'N'; }
  uint8_t stopBits() const override { return 1; }
  bool ok() const override { return true; }

private:
  Mode mode_ = Mode::Valid;
  uint8_t pending_[16] = {};
  size_t pending_len_ = 0;
  uint32_t rx_ = 0;
  uint32_t tx_ = 0;
};

// Native fake Signal-10: CRC-valid Orion status reply (Habr dump body, request addr).
class OrionReplyPort : public Mesh485Port {
public:
  enum class Mode : uint8_t { Valid = 0, BadCrc = 1 };
  void setMode(Mode m) { mode_ = m; }

  size_t writeFrame(const uint8_t* data, size_t len) override {
    tx_ += (uint32_t)len;
    pending_len_ = 0;
    if (!data || len != Signal10Adapter::kStatusReqLen) return len;
    if (!Signal10Adapter::frameCrcOk(data, len)) return len;
    uint8_t r[11];
    memcpy(r, kOrionHabrStatusRsp, 11);
    r[0] = data[0];
    r[10] = crc8_orion(r, 10);
    if (mode_ == Mode::BadCrc) r[10] = (uint8_t)(r[10] ^ 0xFF);
    memcpy(pending_, r, 11);
    pending_len_ = 11;
    return len;
  }
  size_t readFrame(uint8_t* buf, size_t max, uint32_t) override {
    size_t n = pending_len_ < max ? pending_len_ : max;
    if (buf && n) memcpy(buf, pending_, n);
    rx_ += (uint32_t)n;
    pending_len_ = 0;
    return n;
  }
  uint32_t rxCount() const override { return rx_; }
  uint32_t txCount() const override { return tx_; }
  uint32_t baud() const override { return 9600; }
  char parity() const override { return 'N'; }
  uint8_t stopBits() const override { return 1; }
  bool ok() const override { return true; }

private:
  Mode mode_ = Mode::Valid;
  uint8_t pending_[16] = {};
  size_t pending_len_ = 0;
  uint32_t rx_ = 0;
  uint32_t tx_ = 0;
};

static Mesh485Env makeEnv(Mesh485Port* port) {
  Mesh485Env e;
  e.port = port;
  e.radio_ok = true;
  e.repeater = true;
  e.rssi = -80;
  e.snr = 7.5f;
  e.mesh_rx = 1;
  e.mesh_tx = 2;
  e.uptime_s = 12;
  e.heap = 123456;
  e.unix_now = 1787912347;
  e.node_id = MESH485_NODE_ID;
  return e;
}

static const char kNodeStatusReq[] =
    "{\"v\":1,\"type\":\"m485.request\",\"id\":\"8A12F2\",\"node\":\"EXT-001\","
    "\"op\":\"NODE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"c1f0\","
    "\"payload\":{}}";

static const char kTxnReq[] =
    "{\"v\":1,\"type\":\"m485.request\",\"id\":\"B7E401\",\"node\":\"EXT-001\","
    "\"op\":\"RS485_TRANSACTION\",\"operator\":\"admin\",\"ts\":1787912345,"
    "\"nonce\":\"c1f0\",\"payload\":{\"protocol\":\"RAW_RS485\",\"timeout_ms\":900,"
    "\"frame\":\"AQMA\"}}";

int main() {
  // Keep the Access 3MAP stub linked.
  MapMessage mm;
  CHECK(map_parse_json("{\"cmd\":\"STATUS\"}", &mm), "map_parse_json");

  uint8_t raw[8];
  CHECK(mesh485_b64_decode("AQMA", raw, sizeof(raw)) == 3, "b64 decode len");
  CHECK(raw[0] == 0x01 && raw[1] == 0x03 && raw[2] == 0x00, "b64 decode bytes");
  char enc[16];
  CHECK(mesh485_b64_encode(raw, 3, enc, sizeof(enc)) == 4, "b64 encode len");
  CHECK(strcmp(enc, "AQMA") == 0, "b64 encode");

  Mesh485Request req;
  CHECK(mesh485_parse_request(kNodeStatusReq, &req), "parse NODE_STATUS");
  CHECK(req.v == 1, "v");
  CHECK(strcmp(req.id, "8A12F2") == 0, "id");
  CHECK(strcmp(req.node, "EXT-001") == 0, "node");
  CHECK(strcmp(req.op, "NODE_STATUS") == 0, "op");
  CHECK(strcmp(req.optr, "admin") == 0, "operator");

  LoopbackPort bus;
  Mesh485Env env = makeEnv(&bus);
  Mesh485Dispatcher d;
  char out[MESH485_RESP_MAX];

  size_t n = d.handleLine(kNodeStatusReq, env, out, sizeof(out));
  CHECK(n > 0, "NODE_STATUS size");
  CHECK(strstr(out, "\"type\":\"m485.response\"") != nullptr, "resp type");
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "NODE_STATUS OK");
  CHECK(strstr(out, "\"node\":\"EXT-001\"") != nullptr, "node EXT-001");
  CHECK(strstr(out, "\"profile\":\"access+rs485\"") != nullptr, "profile");
  CHECK(strstr(out, "\"radio\":\"ok\"") != nullptr, "radio");
  CHECK(strstr(out, "\"protocol\":\"RAW\"") != nullptr, "rs485 protocol");
  CHECK(strstr(out, "\"session\":\"off\"") != nullptr, "session off");
  CHECK(strstr(out, "\"devices\":6") != nullptr, "NODE_STATUS devices");

  n = d.handleLine(kNodeStatusReq, env, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"REPLAY\"") != nullptr, "replay");

  d.clearReplay();
  n = d.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"00F00D\",\"node\":\"EXT-001\","
      "\"op\":\"NOT_A_CMD\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "unknown op");

  d.clearReplay();
  n = d.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"ACC001\",\"node\":\"EXT-001\","
      "\"op\":\"ACCESS_OPEN\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "ACCESS_OPEN VIEWER");
  CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "ACCESS_OPEN VIEWER mesh_ok");
  CHECK(strstr(out, "\"device_ok\":false") != nullptr, "ACCESS_OPEN VIEWER device_ok");

  d.clearReplay();
  n = d.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CFG001\",\"node\":\"EXT-001\","
      "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env, out, sizeof(out));
  CHECK(strstr(out, "\"baud\":9600") != nullptr, "CONFIG_GET baud");
  CHECK(strstr(out, "\"protocol\":\"RAW\"") != nullptr, "CONFIG_GET protocol RAW");
  CHECK(strstr(out, "BOLID_SIGNAL10") != nullptr, "CONFIG_GET lists SIGNAL10");
  CHECK(strstr(out, "BOLID_SIGNAL20") != nullptr, "CONFIG_GET lists SIGNAL20");
  CHECK(strstr(out, "\"AUTO\"") == nullptr, "CONFIG_GET no AUTO");
  CHECK(strstr(out, "\"mode\":\"MONITOR\"") != nullptr, "CONFIG_GET mode");

  d.clearReplay();
  n = d.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DIA001\",\"node\":\"EXT-001\","
      "\"op\":\"DIAGNOSTICS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env, out, sizeof(out));
  CHECK(strstr(out, "\"rssi\":-80") != nullptr, "DIAG rssi");
  CHECK(strstr(out, "\"heap\":123456") != nullptr, "DIAG heap");
  CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "DIAG mesh_ok");
  CHECK(strstr(out, "\"device_ok\":false") != nullptr, "DIAG device_ok before txn");

  d.clearReplay();
  n = d.handleLine(kTxnReq, env, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "TXN OK");
  CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "mesh_ok");
  CHECK(strstr(out, "\"tx_ok\":true") != nullptr, "tx_ok");
  CHECK(strstr(out, "\"device_ok\":true") != nullptr, "device_ok");
  CHECK(strstr(out, "\"frame\":\"AQMA\"") != nullptr, "frame round-trip");

  EmptyBusPort empty;
  Mesh485Env env_empty = makeEnv(&empty);
  Mesh485Dispatcher d2;
  n = d2.handleLine(kTxnReq, env_empty, out, sizeof(out));
  CHECK(strstr(out, "\"device_ok\":false") != nullptr, "empty device_ok");
  CHECK(strstr(out, "\"error\":\"TIMEOUT\"") != nullptr, "empty TIMEOUT");

  // --- Phase 07 bridge session ---
  LoopbackPort bus3;
  Mesh485Env env3 = makeEnv(&bus3);
  Mesh485Dispatcher d3;
  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0001\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "OPEN omit level OK");
  CHECK(strstr(out, "\"session\":true") != nullptr, "OPEN session true");
  CHECK(strstr(out, "\"level\":\"READ_ONLY\"") != nullptr, "OPEN default READ_ONLY");
  CHECK(d3.sessionOpen(), "session open");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0002\",\"node\":\"EXT-001\","
      "\"op\":\"RS485_TRANSACTION\",\"operator\":\"admin\",\"ts\":1787912345,"
      "\"nonce\":\"x\",\"payload\":{\"protocol\":\"RAW_RS485\",\"timeout_ms\":900,"
      "\"frame\":\"AQMA\"}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "TXN during READ_ONLY");
  CHECK(bus3.txCount() == 0, "READ_ONLY did not TX");

  const uint8_t fake_rx[] = {0x41, 0x42, 0x43};
  d3.captureRx(fake_rx, sizeof(fake_rx), 1787912345);
  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0003\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_POLL\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "POLL OK");
  CHECK(strstr(out, "\"frame\":\"QUJD\"") != nullptr, "POLL base64 ABC");
  CHECK(strstr(out, "\"dropped\":0") != nullptr, "POLL dropped 0");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0004\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"level\":\"READ_ONLY\"}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"BUSY\"") != nullptr, "second OPEN BUSY");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0005\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_CLOSE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "CLOSE OK");
  CHECK(strstr(out, "\"session\":false") != nullptr, "CLOSE session false");
  CHECK(!d3.sessionOpen(), "session closed");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0006\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"level\":\"CONTROL\"}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "CONTROL refused");
  CHECK(!d3.sessionOpen(), "CONTROL did not open");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0007\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"level\":\"RAW_BRIDGE\",\"idle_s\":30}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "RAW OPEN OK");
  CHECK(strstr(out, "\"level\":\"RAW_BRIDGE\"") != nullptr, "RAW level");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0008\",\"node\":\"EXT-001\","
      "\"op\":\"RS485_TRANSACTION\",\"operator\":\"admin\",\"ts\":1787912345,"
      "\"nonce\":\"x\",\"payload\":{\"protocol\":\"RAW_RS485\",\"timeout_ms\":900,"
      "\"frame\":\"AQMA\"}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "RAW TXN OK");
  CHECK(strstr(out, "\"frame\":\"AQMA\"") != nullptr, "RAW TXN loopback");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR0009\",\"node\":\"EXT-001\","
      "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "COMMAND_EXECUTE still refused");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR000A\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_CLOSE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"session\":false") != nullptr, "RAW CLOSE");

  n = d3.handleLine(kTxnReq, env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "TXN after CLOSE (MONITOR)");
  CHECK(strstr(out, "\"frame\":\"AQMA\"") != nullptr, "MONITOR loopback");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR000B\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_CLOSE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "CLOSE idempotent");

  // Idle timeout: OPEN then advance clock past idle_s → session gone, DE RX, TXN ok.
  Mesh485Dispatcher d4;
  LoopbackPort bus4;
  Mesh485Env env4 = makeEnv(&bus4);
  n = d4.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"TO0001\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"level\":\"READ_ONLY\",\"idle_s\":30}}",
      env4, out, sizeof(out));
  CHECK(strstr(out, "\"session\":true") != nullptr, "timeout OPEN");
  bus4.setDirection(true);
  env4.uptime_s = 12 + 30;
  n = d4.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"TO0002\",\"node\":\"EXT-001\","
      "\"op\":\"RS485_TRANSACTION\",\"operator\":\"admin\",\"ts\":1787912345,"
      "\"nonce\":\"x\",\"payload\":{\"protocol\":\"RAW_RS485\",\"timeout_ms\":900,"
      "\"frame\":\"AQMA\"}}",
      env4, out, sizeof(out));
  CHECK(!d4.sessionOpen(), "idle timeout closed session");
  CHECK(!bus4.txDirection(), "DE RX after idle timeout");
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "TXN after idle timeout");

  n = d4.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"TO0003\",\"node\":\"EXT-001\","
      "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env4, out, sizeof(out));
  CHECK(strstr(out, "\"mode\":\"MONITOR\"") != nullptr, "mode MONITOR after timeout");

  n = d3.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"BR000C\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"level\":\"NOT_A_LEVEL\"}}",
      env3, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "unknown level");

  // --- Phase 08 Device / Point / Event ---
  Mesh485Dispatcher d5;
  LoopbackPort bus5;
  Mesh485Env env5 = makeEnv(&bus5);

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0001\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "DISCOVER OK");
  CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "DISCOVER RAW-001");
  CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "DISCOVER TS-001");
  CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "DISCOVER PP-001");
  CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "DISCOVER S10-001");
  CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "DISCOVER S20-001");
  CHECK(strstr(out, "\"protocol\":\"RAW_RS485\"") != nullptr, "DISCOVER RAW_RS485");
  CHECK(strstr(out, "\"protocol\":\"ACCESS\"") != nullptr, "DISCOVER ACCESS");
  CHECK(strstr(out, "\"protocol\":\"BOLID_C2000PP\"") != nullptr, "DISCOVER BOLID_C2000PP");
  CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr, "DISCOVER BOLID_SIGNAL10");
  CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL20\"") != nullptr, "DISCOVER BOLID_SIGNAL20");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0002\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"RAW-001\"}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "INFO OK");
  CHECK(strstr(out, "\"protocol\":\"RAW_RS485\"") != nullptr, "INFO protocol");
  CHECK(strstr(out, "\"capabilities\":[\"READ_STATUS\"]") != nullptr, "INFO caps");
  CHECK(strstr(out, "\"vendor\":\"generic\"") != nullptr, "INFO vendor");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0003\",\"node\":\"EXT-001\","
      "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "POINT_LIST OK");
  CHECK(strstr(out, "\"point_id\":\"BUS\"") != nullptr, "POINT_LIST BUS");
  CHECK(strstr(out, "\"point_id\":\"TURNSTILE\"") != nullptr, "POINT_LIST TURNSTILE");
  CHECK(strstr(out, "\"point_id\":\"READER\"") != nullptr, "POINT_LIST READER");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0004\",\"node\":\"EXT-001\","
      "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"since\":\"0\"}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "EVENT_LIST OK");
  CHECK(strstr(out, "\"events\":[") != nullptr, "EVENT_LIST array");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0005\",\"node\":\"EXT-001\","
      "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "COMMAND_EXECUTE refused");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0006\",\"node\":\"EXT-001\","
      "\"op\":\"EVENT_SUBSCRIBE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "EVENT_SUBSCRIBE OK");
  CHECK(strstr(out, "\"subscribed\":true") != nullptr, "EVENT_SUBSCRIBE subscribed");
  CHECK(strstr(out, "NO_PERMISSION") == nullptr, "EVENT_SUBSCRIBE not refused");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0007\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"NO-SUCH\"}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "unknown device_id");

  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0008\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"level\":\"READ_ONLY\"") != nullptr, "08 OPEN READ_ONLY");
  n = d5.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"DV0009\",\"node\":\"EXT-001\","
      "\"op\":\"RS485_TRANSACTION\",\"operator\":\"admin\",\"ts\":1787912345,"
      "\"nonce\":\"x\",\"payload\":{\"protocol\":\"RAW_RS485\",\"timeout_ms\":900,"
      "\"frame\":\"AQMA\"}}",
      env5, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "08 TXN blocked");
  CHECK(bus5.txCount() == 0, "08 READ_ONLY did not TX");

  // --- Phase 10 С2000-ПП Modbus READ_ONLY ---
  {
    uint8_t probe[8] = {0x01, 0x03, 0xB4, 0x48, 0x00, 0x02, 0, 0};
    CHECK(crc16_modbus_append(probe, 6, sizeof(probe)) == 8, "probe ADU len");
    CHECK(crc16_modbus_ok(probe, 8), "probe CRC-16");
    CHECK(probe[6] == 0x63 && probe[7] == 0xED, "probe CRC bytes LE");
    uint8_t bad[8];
    memcpy(bad, probe, 8);
    bad[7] = (uint8_t)(bad[7] ^ 0xFF);
    CHECK(!crc16_modbus_ok(bad, 8), "flipped CRC fails");
    CHECK(crc8_orion(probe, 6) != probe[6], "crc8_orion is not Modbus CRC");
  }

  EmptyBusPort empty10;
  Mesh485Env env10 = makeEnv(&empty10);
  Mesh485Dispatcher d10;
  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0001\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env10, out, sizeof(out));
  CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "10 DISCOVER RAW");
  CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "10 DISCOVER TS");
  CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "10 DISCOVER PP");
  CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "10 DISCOVER S10");
  CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "10 DISCOVER S20");

  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0002\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env10, out, sizeof(out));
  CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "10 INFO id");
  CHECK(strstr(out, "\"vendor\":\"bolid\"") != nullptr, "10 INFO vendor");
  CHECK(strstr(out, "\"model\":\"C2000PP\"") != nullptr, "10 INFO model");
  CHECK(strstr(out, "\"protocol\":\"BOLID_C2000PP\"") != nullptr, "10 INFO proto");
  CHECK(strstr(out, "\"online\":false") != nullptr, "10 INFO offline before probe");
  CHECK(strstr(out, "\"capabilities\":[\"READ_STATUS\"]") != nullptr, "10 INFO caps");
  CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "10 no CONTROL_RELAY");

  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0003\",\"node\":\"EXT-001\","
      "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env10, out, sizeof(out));
  CHECK(strstr(out, "\"events\":[]") != nullptr, "10 EVENT_LIST empty");
  CHECK(strstr(out, "FIRE") == nullptr, "10 no fake FIRE");

  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0004\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env10, out, sizeof(out));
  CHECK(empty10.txCount() == 8, "10 empty bus probe TX (Modbus only)");
  CHECK(strstr(out, "\"online\":false") != nullptr, "10 STATUS still offline (in-flight)");
  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs +
                     C2000ppModbusAdapter::kTxnTimeoutMs);
  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0005\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env10, out, sizeof(out));
  CHECK(strstr(out, "\"online\":false") != nullptr, "10 timeout online false");

  n = d10.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0006\",\"node\":\"EXT-001\","
      "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env10, out, sizeof(out));
  CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "10 COMMAND_EXECUTE refused");

  // Loopback echoes the FC03 request — not a Modbus reply.
  LoopbackPort loop10;
  Mesh485Env env_loop = makeEnv(&loop10);
  Mesh485Dispatcher d_loop;
  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
  n = d_loop.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0007\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env_loop, out, sizeof(out));
  CHECK(strstr(out, "\"online\":false") != nullptr, "10 loopback not a reply");

  ModbusReplyPort ok_bus;
  Mesh485Env env_ok = makeEnv(&ok_bus);
  Mesh485Dispatcher d_ok;
  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
  n = d_ok.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0008\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env_ok, out, sizeof(out));
  CHECK(strstr(out, "\"online\":true") != nullptr, "10 valid FC03 online");
  CHECK(strstr(out, "\"protocol\":\"BOLID_C2000PP\"") != nullptr, "10 online proto");

  ModbusReplyPort bad_bus;
  bad_bus.setMode(ModbusReplyPort::Mode::BadCrc);
  Mesh485Env env_bad = makeEnv(&bad_bus);
  Mesh485Dispatcher d_bad;
  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
  n = d_bad.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP0009\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env_bad, out, sizeof(out));
  CHECK(strstr(out, "\"online\":false") != nullptr, "10 flipped CRC offline");
  n = d_bad.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP000A\",\"node\":\"EXT-001\","
      "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env_bad, out, sizeof(out));
  CHECK(strstr(out, "\"point_id\":\"PP\"") != nullptr, "10 diagnostic PP point");
  CHECK(strstr(out, "\"point_id\":\"Z1\"") == nullptr, "10 no zone from bad CRC");

  Mesh485Dispatcher d_br;
  EmptyBusPort br_bus;
  Mesh485Env env_br = makeEnv(&br_bus);
  mesh485_set_now_ms(0);
  n = d_br.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP000B\",\"node\":\"EXT-001\","
      "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env_br, out, sizeof(out));
  CHECK(strstr(out, "\"level\":\"READ_ONLY\"") != nullptr, "10 OPEN READ_ONLY");
  const uint32_t tx_open = br_bus.txCount();
  mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
  n = d_br.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP000C\",\"node\":\"EXT-001\","
      "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{}}",
      env_br, out, sizeof(out));
  CHECK(br_bus.txCount() == tx_open, "10 READ_ONLY adapter did not TX");
  CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "10 DISCOVER during session");

  n = d_ok.handleLine(
      "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PP000D\",\"node\":\"EXT-001\","
      "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
      "\"payload\":{\"device_id\":\"PP-001\"}}",
      env_ok, out, sizeof(out));
  CHECK(strstr(out, "FIRE") == nullptr, "10 online EVENT_LIST no FIRE");

  mesh485_set_now_ms(0);

  // --- Phase 11 event queue + dual ACK ---
  {
    Mesh485Dispatcher dq;
    LoopbackPort busq;
    Mesh485Env envq = makeEnv(&busq);
    dq.devices().access().noteCard("DEADBEEF", 42, "ENTRY", "Authorized", "Stub");
    n = dq.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"EQ0001\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"since\":\"0\"}}",
        envq, out, sizeof(out));
    CHECK(strstr(out, "\"type\":\"CARD\"") != nullptr, "11 EVENT_LIST CARD");
    CHECK(strstr(out, "\"source\":\"RFID\"") != nullptr, "11 source RFID");
    CHECK(strstr(out, "DEADBEEF") != nullptr, "11 rfid uid");
    CHECK(strstr(out, "\"mesh_ok\":false") != nullptr, "11 mesh_ok pending");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "11 device_ok local");

    n = dq.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"EQ0002\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_SUBSCRIBE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        envq, out, sizeof(out));
    CHECK(strstr(out, "\"subscribed\":true") != nullptr, "11 SUBSCRIBE");

    uint32_t seq = 0;
    char line[MESH485_RESP_MAX];
    const size_t ln = dq.nextPendingEventLine(line, sizeof(line), &seq);
    CHECK(ln > 0, "11 pending NDJSON");
    CHECK(strstr(line, "\"type\":\"m485.event\"") != nullptr, "11 m485.event");
    CHECK(strstr(line, "\"mesh_ok\":true") != nullptr, "11 emit mesh ACK");
    CHECK(dq.ackMesh(seq), "11 ackMesh");

    n = dq.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"EQ0003\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"since\":\"0\"}}",
        envq, out, sizeof(out));
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "11 listed mesh_ok after ACK");

    n = dq.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"EQ0004\",\"node\":\"EXT-001\","
        "\"op\":\"DIAGNOSTICS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        envq, out, sizeof(out));
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "11 DIAG mesh_ok");
    CHECK(strstr(out, "\"device_ok\"") != nullptr, "11 DIAG device_ok field");
    CHECK(strstr(out, "\"pending_mesh\"") != nullptr, "11 DIAG pending_mesh");
  }

  event_store_reset_fake_fs();
  event_store_use_fake_fs(true);
  {
    EventStore a;
    a.begin();
    EventRecord rec;
    event_record_clear(&rec);
    event_record_copy_str(rec.device_id, sizeof(rec.device_id), "TS-001");
    event_record_copy_str(rec.point_id, sizeof(rec.point_id), "READER");
    event_record_copy_str(rec.type, sizeof(rec.type), "CARD");
    event_record_copy_str(rec.source, sizeof(rec.source), "RFID");
    event_record_copy_str(rec.rfid_uid, sizeof(rec.rfid_uid), "REBOOT1");
    rec.device_ok = 1;
    CHECK(a.append(rec), "11 persist append");
    CHECK(a.count() == 1, "11 persist count");
  }
  {
    EventStore b;
    b.begin();
    CHECK(b.count() == 1, "11 survive simulated reboot");
    EventRecord rec;
    CHECK(b.getAt(0, &rec), "11 reboot get");
    CHECK(strcmp(rec.rfid_uid, "REBOOT1") == 0, "11 reboot uid");
    CHECK(rec.device_ok == 1, "11 reboot device_ok");
    CHECK(rec.mesh_ok == 0, "11 reboot mesh still pending");
  }
  event_store_use_fake_fs(false);
  event_store_reset_fake_fs();

  // --- Phase 14 COMMAND_EXECUTE + capabilities / roles ---
  {
    CHECK(mesh485_role("viewer") == Mesh485Role::Viewer, "14 role viewer");
    CHECK(mesh485_role("operator") == Mesh485Role::Operator, "14 role operator");
    CHECK(mesh485_role("admin") == Mesh485Role::Admin, "14 role admin");
    CHECK(!mesh485_can_execute(Mesh485Role::Viewer), "14 viewer cannot execute");
    CHECK(mesh485_can_execute(Mesh485Role::Operator), "14 operator can execute");
    CHECK(mesh485_is_forbidden_cap("DISABLE_FIRE_ALARM"), "14 fire-disable forbidden");
    CHECK(mesh485_is_forbidden_cap("DISARM_ALL"), "14 disarm-all forbidden");
    CHECK(mesh485_is_executable_cap("CONTROL_RELAY"), "14 CONTROL_RELAY executable");
    CHECK(!mesh485_is_executable_cap("READ_STATUS"), "14 READ_STATUS not an op");
  }

  {
    EmptyBusPort bus14;
    Mesh485Env env14 = makeEnv(&bus14);
    Mesh485Dispatcher d14;

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0001\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"DISABLE_FIRE_ALARM\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 fire-disable refused");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "14 fire-disable mesh ACK");
    CHECK(strstr(out, "\"device_ok\":false") != nullptr, "14 fire-disable no device ACK");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0002\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"DISARM_ALL\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 disarm-all refused");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0003\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 no cap until allow-list");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0004\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "14 default no CONTROL_RELAY");

    d14.devices().c2000pp().allowRelay(1, true);

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0005\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "CONTROL_RELAY") != nullptr, "14 INFO CONTROL_RELAY after allow");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0006\",\"node\":\"EXT-001\","
        "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"point_id\":\"R1\"") != nullptr, "14 named R1 point");
    CHECK(strstr(out, "\"type\":\"relay\"") != nullptr, "14 R1 type relay");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0007\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 VIEWER refused");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0008\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"TIMEOUT\"") != nullptr, "14 empty bus TIMEOUT");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "14 timeout mesh ACK");
    CHECK(strstr(out, "\"device_ok\":false") != nullptr, "14 timeout no device ACK");
    CHECK(bus14.txCount() >= 8, "14 named relay did TX FC05");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX0009\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R9\",\"action\":\"ON\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 R9 not mapped");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX000A\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"RAW-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "14 RAW has no CONTROL_RELAY");

    n = d14.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX000B\",\"node\":\"EXT-001\","
        "\"op\":\"DIAGNOSTICS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env14, out, sizeof(out));
    CHECK(strstr(out, "\"last_command\"") != nullptr, "14 DIAG last_command");
    CHECK(strstr(out, "CONTROL_RELAY") != nullptr, "14 DIAG last cap");
  }

  {
    LoopbackPort bus_ok;
    Mesh485Env env_ok = makeEnv(&bus_ok);
    Mesh485Dispatcher d_ok;
    d_ok.devices().c2000pp().allowRelay(1, true);
    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CX000C\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "14 loopback CONTROL OK");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "14 loopback mesh ACK");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "14 loopback device ACK");
    CHECK(strstr(out, "\"error\":null") != nullptr, "14 loopback error null");
  }

  // --- Phase 15 Signal-10 MONITOR (Orion status-read; no CONTROL) ---
  mesh485_set_now_ms(0);
  {
    uint8_t req[Signal10Adapter::kStatusReqLen];
    CHECK(Signal10Adapter::buildStatusRequest(req, sizeof(req), 3) == 9, "15 dump req len");
    CHECK(memcmp(req, kOrionHabrStatusReq, 9) == 0, "15 dump status req addr 3");
    CHECK(Signal10Adapter::frameCrcOk(kOrionHabrStatusRsp, sizeof(kOrionHabrStatusRsp)),
          "15 dump rsp CRC");
    uint8_t flipped[9];
    memcpy(flipped, req, 9);
    flipped[8] = (uint8_t)(flipped[8] ^ 0xFF);
    CHECK(!Signal10Adapter::frameCrcOk(flipped, 9), "15 flipped CRC fails");
  }

  {
    EmptyBusPort empty15;
    Mesh485Env env15 = makeEnv(&empty15);
    Mesh485Dispatcher d15;
    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1001\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "15 viewer CONFIG_SET refused");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1002\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"AUTO\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "15 AUTO refused");
    CHECK(strstr(out, "no AUTO") != nullptr, "15 AUTO payload");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1003\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "15 empty CONFIG_SET persist refused");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1004\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "15 CONFIG_SET SIGNAL10");
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr, "15 CONFIG_GET echo");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1005\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "15 INFO id");
    CHECK(strstr(out, "\"model\":\"SIGNAL10\"") != nullptr, "15 INFO model");
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr, "15 INFO proto");
    CHECK(strstr(out, "\"online\":false") != nullptr, "15 INFO offline before probe");
    CHECK(strstr(out, "\"capabilities\":[\"READ_STATUS\"]") != nullptr, "15 INFO caps");
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "15 no CONTROL_RELAY");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1006\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"events\":[]") != nullptr, "15 EVENT_LIST empty");
    CHECK(strstr(out, "FIRE") == nullptr, "15 no fake FIRE");

    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs);
    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1007\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env15, out, sizeof(out));
    CHECK(empty15.txCount() == Signal10Adapter::kStatusReqLen, "15 empty bus Orion probe");
    CHECK(strstr(out, "\"online\":false") != nullptr, "15 STATUS still offline (in-flight)");
    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs + Signal10Adapter::kTxnTimeoutMs);
    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1008\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"online\":false") != nullptr, "15 timeout online false");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1009\",\"node\":\"EXT-001\","
        "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"point_id\":\"S10\"") != nullptr, "15 diagnostic S10 point");
    CHECK(strstr(out, "\"point_id\":\"Z1\"") == nullptr, "15 no zone on empty bus");

    n = d15.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100A\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env15, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "15 COMMAND_EXECUTE refused");
    mesh485_set_now_ms(0);
  }

  {
    OrionReplyPort ok15;
    Mesh485Env env_ok = makeEnv(&ok15);
    Mesh485Dispatcher d_ok;
    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100B\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "15 online CONFIG_SET");
    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs);
    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100C\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"online\":true") != nullptr, "15 valid Orion online");
    CHECK(strstr(out, "READ_ZONES") != nullptr, "15 READ_ZONES after status");
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "15 online no CONTROL");

    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100D\",\"node\":\"EXT-001\","
        "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"point_id\":\"Z1\"") != nullptr, "15 Z1 after CRC-valid reply");
    CHECK(strstr(out, "\"state\":\"UNKNOWN\"") != nullptr, "15 zone not invented ALARM");

    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100E\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "FIRE") == nullptr, "15 online EVENT_LIST no FIRE");
    mesh485_set_now_ms(0);
  }

  {
    OrionReplyPort bad15;
    bad15.setMode(OrionReplyPort::Mode::BadCrc);
    Mesh485Env env_bad = makeEnv(&bad15);
    Mesh485Dispatcher d_bad;
    n = d_bad.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S100F\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env_bad, out, sizeof(out));
    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs);
    n = d_bad.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1010\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S10-001\"}}",
        env_bad, out, sizeof(out));
    CHECK(strstr(out, "\"online\":false") != nullptr, "15 flipped CRC offline");
    mesh485_set_now_ms(0);
  }

  {
    Mesh485Dispatcher d_br;
    EmptyBusPort br_bus;
    Mesh485Env env_br = makeEnv(&br_bus);
    mesh485_set_now_ms(0);
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1011\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env_br, out, sizeof(out));
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1012\",\"node\":\"EXT-001\","
        "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env_br, out, sizeof(out));
    CHECK(strstr(out, "\"level\":\"READ_ONLY\"") != nullptr, "15 OPEN READ_ONLY");
    const uint32_t tx_open = br_bus.txCount();
    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs);
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S1013\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env_br, out, sizeof(out));
    CHECK(br_bus.txCount() == tx_open, "15 READ_ONLY adapter did not TX");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "15 DISCOVER during session");
    mesh485_set_now_ms(0);
  }

  // --- Phase 16 Signal-20 MONITOR (same Orion status-read; no CONTROL) ---
  mesh485_set_now_ms(0);
  {
    uint8_t req[Signal20Adapter::kStatusReqLen];
    CHECK(Signal20Adapter::buildStatusRequest(req, sizeof(req), 3) == 9, "16 dump req len");
    CHECK(memcmp(req, kOrionHabrStatusReq, 9) == 0, "16 dump status req addr 3");
    CHECK(Signal20Adapter::frameCrcOk(kOrionHabrStatusRsp, sizeof(kOrionHabrStatusRsp)),
          "16 dump rsp CRC");
    uint8_t flipped[9];
    memcpy(flipped, req, 9);
    flipped[8] = (uint8_t)(flipped[8] ^ 0xFF);
    CHECK(!Signal20Adapter::frameCrcOk(flipped, 9), "16 flipped CRC fails");
  }

  {
    EmptyBusPort empty16;
    Mesh485Env env16 = makeEnv(&empty16);
    Mesh485Dispatcher d16;
    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2001\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "16 viewer CONFIG_SET refused");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2002\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"AUTO\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "16 AUTO refused");
    CHECK(strstr(out, "no AUTO") != nullptr, "16 AUTO payload");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2003\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "16 CONFIG_SET SIGNAL20");
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL20\"") != nullptr, "16 CONFIG_GET echo");
    CHECK(strstr(out, "BOLID_SIGNAL10") != nullptr, "16 protocols still list SIGNAL10");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2004\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "16 INFO id");
    CHECK(strstr(out, "\"model\":\"SIGNAL20\"") != nullptr, "16 INFO model");
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL20\"") != nullptr, "16 INFO proto");
    CHECK(strstr(out, "\"online\":false") != nullptr, "16 INFO offline before probe");
    CHECK(strstr(out, "\"capabilities\":[\"READ_STATUS\"]") != nullptr, "16 INFO caps");
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "16 no CONTROL_RELAY");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2005\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"events\":[]") != nullptr, "16 EVENT_LIST empty");
    CHECK(strstr(out, "FIRE") == nullptr, "16 no fake FIRE");

    mesh485_set_now_ms(Signal20Adapter::kPollIntervalMs);
    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2006\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env16, out, sizeof(out));
    CHECK(empty16.txCount() == Signal20Adapter::kStatusReqLen, "16 empty bus Orion probe");
    CHECK(strstr(out, "\"online\":false") != nullptr, "16 STATUS still offline (in-flight)");
    mesh485_set_now_ms(Signal20Adapter::kPollIntervalMs + Signal20Adapter::kTxnTimeoutMs);
    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2007\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"online\":false") != nullptr, "16 timeout online false");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2008\",\"node\":\"EXT-001\","
        "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"point_id\":\"S20\"") != nullptr, "16 diagnostic S20 point");
    CHECK(strstr(out, "\"point_id\":\"Z1\"") == nullptr, "16 no zone on empty bus");
    CHECK(strstr(out, "FIRE") == nullptr, "16 points no FIRE");

    n = d16.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2009\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env16, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "16 COMMAND_EXECUTE refused");
    mesh485_set_now_ms(0);
  }

  {
    // Default RAW: S20 does not TX; PP still sends 8-byte Modbus.
    EmptyBusPort raw16;
    Mesh485Env env_raw = makeEnv(&raw16);
    Mesh485Dispatcher d_raw;
    mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
    n = d_raw.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200A\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env_raw, out, sizeof(out));
    CHECK(raw16.txCount() == 8, "16 default RAW is PP Modbus only");
    CHECK(strstr(out, "\"online\":false") != nullptr, "16 S20 offline under RAW");
    mesh485_set_now_ms(0);
  }

  {
    OrionReplyPort ok16;
    Mesh485Env env_ok = makeEnv(&ok16);
    Mesh485Dispatcher d_ok;
    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200B\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "16 online CONFIG_SET");
    mesh485_set_now_ms(Signal20Adapter::kPollIntervalMs);
    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200C\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"online\":true") != nullptr, "16 valid Orion online");
    CHECK(strstr(out, "READ_ZONES") != nullptr, "16 READ_ZONES after status");
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "16 online no CONTROL");

    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200D\",\"node\":\"EXT-001\","
        "\"op\":\"POINT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "\"point_id\":\"Z1\"") != nullptr, "16 Z1 after CRC-valid reply");
    CHECK(strstr(out, "\"state\":\"UNKNOWN\"") != nullptr, "16 zone not invented ALARM");
    CHECK(strstr(out, "FIRE") == nullptr, "16 online points no FIRE");

    n = d_ok.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200E\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env_ok, out, sizeof(out));
    CHECK(strstr(out, "FIRE") == nullptr, "16 online EVENT_LIST no FIRE");
    mesh485_set_now_ms(0);
  }

  {
    OrionReplyPort bad16;
    bad16.setMode(OrionReplyPort::Mode::BadCrc);
    Mesh485Env env_bad = makeEnv(&bad16);
    Mesh485Dispatcher d_bad;
    n = d_bad.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S200F\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env_bad, out, sizeof(out));
    mesh485_set_now_ms(Signal20Adapter::kPollIntervalMs);
    n = d_bad.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2010\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\"}}",
        env_bad, out, sizeof(out));
    CHECK(strstr(out, "\"online\":false") != nullptr, "16 flipped CRC offline");
    mesh485_set_now_ms(0);
  }

  {
    Mesh485Dispatcher d_br;
    EmptyBusPort br_bus;
    Mesh485Env env_br = makeEnv(&br_bus);
    mesh485_set_now_ms(0);
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2011\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env_br, out, sizeof(out));
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2012\",\"node\":\"EXT-001\","
        "\"op\":\"BRIDGE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env_br, out, sizeof(out));
    CHECK(strstr(out, "\"level\":\"READ_ONLY\"") != nullptr, "16 OPEN READ_ONLY");
    const uint32_t tx_open = br_bus.txCount();
    mesh485_set_now_ms(Signal20Adapter::kPollIntervalMs);
    n = d_br.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"S2013\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env_br, out, sizeof(out));
    CHECK(br_bus.txCount() == tx_open, "16 READ_ONLY adapter did not TX");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "16 DISCOVER during session");
    mesh485_set_now_ms(0);
  }

  // --- Phase 17 Access ACL + 3MAP ---
  {
    AccessController a;
    a.begin();
    CHECK(a.checkUid("DEADBEEF").result == AccessResult::Deny, "17 empty ACL DENY");
    CHECK(a.checkUid("").result == AccessResult::Deny, "17 empty uid DENY");
    CHECK(a.checkUid(nullptr).result == AccessResult::Deny, "17 null uid DENY");
    CHECK(a.allowUid("aa bb:cc-dd"), "17 allowUid canonical");
    CHECK(a.checkUid("AABBCCDD").result == AccessResult::Allow, "17 allowlisted ALLOW");
    CHECK(a.enabledCount() == 1, "17 enabled count");
    CHECK(a.checkUid("DEADBEEF").result == AccessResult::Deny, "17 unknown still DENY");
    CHECK(a.addCard("AABBCCDD", "E1", "Lab", false), "17 disable");
    CHECK(a.checkUid("AABBCCDD").result == AccessResult::Deny, "17 disabled DENY");
    CHECK(a.enabledCount() == 0, "17 disabled not counted");
    bool overflow = false;
    for (int i = 0; i < 80; ++i) {
      char uid[24];
      snprintf(uid, sizeof(uid), "%08X", (unsigned)(i + 1));
      if (!a.addCard(uid, "", "", true)) {
        overflow = true;
        break;
      }
    }
    CHECK(overflow, "17 ACL overflow refused");
    CHECK(a.count() == ACCESS_ACL_CAP, "17 ACL cap");
  }

  access_acl_reset_fake_fs();
  access_acl_use_fake_fs(true);
  {
    AccessController a;
    a.begin();
    CHECK(a.addCard("04A79122", "E1", "Lab", true), "17 persist add");
  }
  {
    AccessController b;
    b.begin();
    CHECK(b.checkUid("04A79122").result == AccessResult::Allow, "17 persist reboot");
    CHECK(b.enabledCount() == 1, "17 persist users");
  }
  access_acl_use_fake_fs(false);
  access_acl_reset_fake_fs();

  {
    Mesh485Dispatcher d17;
    EmptyBusPort bus17;
    Mesh485Env env17 = makeEnv(&bus17);
    const uint32_t tx0 = bus17.txCount();

    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17001\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "17 VIEWER OPEN refused");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "17 VIEWER OPEN mesh_ok");
    CHECK(strstr(out, "\"device_ok\":false") != nullptr, "17 VIEWER OPEN device_ok");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17002\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"pulse_ms\":300}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 OPERATOR OPEN OK");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "17 OPEN mesh_ok");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "17 OPEN device_ok fake HAL");
    CHECK(d17.gate().lastOpenOk(), "17 fake HAL pulsed");
    CHECK(d17.gate().lastRemote(), "17 showRemote flag");
    CHECK(bus17.txCount() == tx0, "17 ACCESS_OPEN did not TX UART1");

    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17003\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"TS-001\",\"since\":\"0\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"type\":\"REMOTE_OPEN\"") != nullptr, "17 EVENT_LIST REMOTE_OPEN");
    CHECK(strstr(out, "\"source\":\"USB\"") != nullptr, "17 REMOTE_OPEN source USB");
    CHECK(strstr(out, "00000000-0000-4000-8000-") != nullptr, "17 REMOTE_OPEN UUID");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17004\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_GET_EVENTS\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"since\":\"0\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 ACCESS_GET_EVENTS OK");
    CHECK(strstr(out, "\"type\":\"REMOTE_OPEN\"") != nullptr, "17 GET_EVENTS REMOTE_OPEN");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17005\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_TURNSTILE_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 alias ACCESS_TURNSTILE_OPEN");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17006\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_LOCK\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 ACCESS_LOCK OK");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "17 LOCK device_ok");
    CHECK(strstr(out, "LOCKED") != nullptr, "17 LOCK state");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17007\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_LOCK\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "17 VIEWER LOCK refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17008\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_STATUS\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 ACCESS_STATUS OK");
    CHECK(strstr(out, "\"device\":\"TS-001\"") != nullptr, "17 STATUS device");
    CHECK(strstr(out, "\"users\":0") != nullptr, "17 STATUS users empty ACL");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17009\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_ADD_CARD\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"04A79122\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "17 VIEWER ADD_CARD refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700A\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_ADD_CARD\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"04A79122\",\"employee_id\":\"E1\",\"name\":\"Lab\","
        "\"enabled\":true}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 ADMIN ADD_CARD");
    CHECK(d17.acl().checkUid("04A79122").result == AccessResult::Allow, "17 ACL hit after add");
    CHECK(d17.acl().checkUid("DEADBEEF").result == AccessResult::Deny, "17 unknown still DENY");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700B\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_STATUS\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"users\":1") != nullptr, "17 STATUS users after add");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700D\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_REMOVE_CARD\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"04A79122\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 REMOVE_CARD");
    CHECK(d17.acl().checkUid("04A79122").result == AccessResult::Deny, "17 removed is DENY");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700E\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"TS-001\",\"capability\":\"ACCESS_OPEN\","
        "\"point_id\":\"TURNSTILE\",\"action\":\"ON\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "17 COMMAND_EXECUTE not OPEN");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700F\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OTA\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "17 ACCESS_OTA refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17010\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_FIRE_DISABLE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "17 ACCESS fire-disable refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17011\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"TS-001\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "ACCESS_OPEN") != nullptr, "17 TS-001 advertises ACCESS_OPEN");
    CHECK(strstr(out, "CONTROL_RELAY") == nullptr, "17 TS-001 no CONTROL_RELAY");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17012\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "17 still RAW-001");
    CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "17 still TS-001");
    CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "17 still PP-001");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "17 still S10-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "17 still S20-001");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17013\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"S20-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "17 S20 CONTROL still refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17014\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"AUTO\"}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "17 AUTO still refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A18001\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_GET_USERS\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "18 VIEWER GET_USERS refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A18002\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_GET_USERS\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "18 OPERATOR GET_USERS refused");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A18003\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_ADD_CARD\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"CAFEBABE\",\"employee_id\":\"E18\",\"name\":\"Ivan\","
        "\"enabled\":true}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "18 ADMIN ADD_CARD for GET_USERS");

    d17.clearReplay();
    n = d17.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A18004\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_GET_USERS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env17, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "18 ADMIN GET_USERS OK");
    CHECK(strstr(out, "CAFEBABE") != nullptr, "18 GET_USERS returns uid");
    CHECK(strstr(out, "\"employee_id\":\"E18\"") != nullptr, "18 GET_USERS employee_id");
    CHECK(strstr(out, "\"name\":\"Ivan\"") != nullptr, "18 GET_USERS name");
    CHECK(strstr(out, "base64") == nullptr, "18 GET_USERS no photo bytes");
    CHECK(strstr(out, ".jpg") == nullptr, "18 GET_USERS no jpeg path on wire");
    CHECK(d17.acl().checkUid("DEADBEEF").result == AccessResult::Deny, "18 unknown still DENY");
    CHECK(bus17.txCount() == tx0, "18 ACCESS_GET_USERS did not TX UART1");
  }

  {
    Mesh485Dispatcher dc;
    EmptyBusPort bc;
    Mesh485Env ec = makeEnv(&bc);
    dc.devices().access().noteCard("DEADBEEF", 42, "ENTRY", "Denied", "");
    n = dc.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700C\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_GET_EVENTS\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"since\":\"0\"}}",
        ec, out, sizeof(out));
    CHECK(strstr(out, "\"type\":\"CARD\"") != nullptr, "17 card event in GET_EVENTS");
    CHECK(strstr(out, "DEADBEEF") != nullptr, "17 denied uid queued");
    CHECK(strstr(out, "Denied") != nullptr, "17 denied status");
    CHECK(strstr(out, "00000000-0000-4000-8000-") != nullptr, "17 card UUID");
    n = dc.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A1700C2\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"TS-001\",\"since\":\"0\"}}",
        ec, out, sizeof(out));
    CHECK(strstr(out, "DEADBEEF") != nullptr, "17 EVENT_LIST same card");
  }

  access_acl_reset_fake_fs();
  access_acl_use_fake_fs(true);
  {
    Mesh485Dispatcher da;
    EmptyBusPort ba;
    Mesh485Env ea = makeEnv(&ba);
    n = da.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"A17015\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_ADD_CARD\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"AABBCCDD\"}}",
        ea, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "17 ADD_CARD fake FS");
  }
  {
    Mesh485Dispatcher db;
    CHECK(db.acl().checkUid("AABBCCDD").result == AccessResult::Allow, "17 ADD_CARD reboot");
  }
  access_acl_use_fake_fs(false);
  access_acl_reset_fake_fs();

  // --- Phase 20 local AuthZ (RAM allow-list + BAD_AUTH) ---
  {
    CHECK(mesh485_parse_role("ADMIN", nullptr) == false, "20 parse_role needs out");
    Mesh485Role pr = Mesh485Role::Viewer;
    CHECK(mesh485_parse_role("ADMIN", &pr) && pr == Mesh485Role::Admin, "20 parse ADMIN");
    CHECK(mesh485_ieq("Admin", "admin"), "20 ieq");

    Mesh485Dispatcher d20;
    EmptyBusPort bus20;
    Mesh485Env env20 = makeEnv(&bus20);
    CHECK(d20.auth().count() == 3, "20 factory three names");
    Mesh485Role stored = Mesh485Role::Viewer;
    CHECK(d20.auth().resolve("admin", &stored) && stored == Mesh485Role::Admin,
          "20 factory admin");
    CHECK(d20.auth().resolve("operator", &stored) && stored == Mesh485Role::Operator,
          "20 factory operator");
    CHECK(d20.auth().resolve("viewer", &stored) && stored == Mesh485Role::Viewer,
          "20 factory viewer");
    CHECK(!d20.auth().resolve("eve", &stored), "20 unknown eve");

    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0001\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "20 VIEWER OPEN NO_PERMISSION");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "20 VIEWER OPEN mesh_ok");
    CHECK(strstr(out, "\"device_ok\":false") != nullptr, "20 VIEWER OPEN device_ok");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0002\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"eve\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "20 unknown OPEN BAD_AUTH");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0003\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_ADD_CARD\",\"operator\":\"eve\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"uid\":\"04A79122\"}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "20 unknown ADD_CARD BAD_AUTH");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0004\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"eve\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PP-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"R1\",\"action\":\"ON\"}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "20 unknown COMMAND BAD_AUTH");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0005\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "20 factory admin OPEN OK");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "20 factory admin mesh_ok");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "20 factory admin device_ok fake HAL");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0006\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"role\":\"operator\"}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "20 VIEWER AUTH_SET refused");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0007\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"role\":\"operator\"}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "20 ADMIN AUTH_SET lab");
    CHECK(d20.auth().lookup("lab", &stored) && stored == Mesh485Role::Operator,
          "20 lab stored operator");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0008\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "20 AUTH_SET lab OPEN");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ0009\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"operator\",\"role\":\"viewer\"}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "20 AUTH_SET downgrade operator");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ000A\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"operator\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "20 claimed role above list");

    d20.clearReplay();
    CHECK(d20.auth().remove("admin"), "20 test-hook drop factory admin");
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ000B\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "20 spoof admin without row");

    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ000B\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"REPLAY\"") != nullptr, "20 replay still first");

    d20.clearReplay();
    n = d20.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AZ000D\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env20, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "20 still RAW-001");
    CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "20 still TS-001");
    CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "20 still PP-001");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "20 still S10-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "20 still S20-001");
  }

  // --- Phase 21 persist allow-list (SPIFFS blob / native fake-fs) ---
  mesh485_auth_reset_fake_fs();
  mesh485_auth_use_fake_fs(true);
  {
    Mesh485Dispatcher d21;
    EmptyBusPort bus21;
    Mesh485Env env21 = makeEnv(&bus21);
    n = d21.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0001\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"role\":\"operator\"}}",
        env21, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "21 AUTH_SET lab persist");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    Mesh485Role stored = Mesh485Role::Viewer;
    CHECK(reboot.auth().lookup("lab", &stored) && stored == Mesh485Role::Operator,
          "21 lab survives begin()");
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0002\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "21 lab OPEN after reboot");
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "21 lab OPEN mesh_ok");
  }
  {
    Mesh485Dispatcher d21b;
    d21b.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d21b.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0003\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"root\",\"role\":\"admin\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "21 second admin");
    d21b.clearReplay();
    n = d21b.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0004\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"root\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"admin\",\"enabled\":false}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "21 remove factory admin persist");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    CHECK(!reboot.auth().lookup("admin", nullptr), "21 factory admin gone after persist");
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0005\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "21 spoof admin after remove+persist");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"AP0006\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "21 lab still OPEN");
  }
  {
    uint8_t junk[8];
    memset(junk, 0xFF, sizeof(junk));
    mesh485_auth_poke_fake_fs(junk, sizeof(junk));
    Mesh485Dispatcher bad;
    bad.beginStore();
    CHECK(bad.auth().count() == 3, "21 bad magic → factory count");
    Mesh485Role stored = Mesh485Role::Viewer;
    CHECK(bad.auth().resolve("admin", &stored) && stored == Mesh485Role::Admin,
          "21 bad magic → factory admin");
    CHECK(bad.auth().resolve("viewer", &stored) && stored == Mesh485Role::Viewer,
          "21 bad magic → factory viewer");
  }
  {
    Mesh485AuthHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = Mesh485Auth::kMagic;
    h.version = Mesh485Auth::kVersionV1;
    h.count = 1;
    h.cap = MESH485_AUTH_CAP;
    Mesh485AuthRecord recs[MESH485_AUTH_CAP];
    memset(recs, 0, sizeof(recs));
    memcpy(recs[0].name, "eve", 4);
    recs[0].role = (uint8_t)Mesh485Role::Viewer;
    uint8_t blob[sizeof(h) + sizeof(recs)];
    memcpy(blob, &h, sizeof(h));
    memcpy(blob + sizeof(h), recs, sizeof(recs));
    mesh485_auth_poke_fake_fs(blob, sizeof(blob));
    Mesh485Dispatcher noadm;
    noadm.beginStore();
    CHECK(noadm.auth().count() == 3, "21 no-ADMIN file → factory");
    Mesh485Role stored = Mesh485Role::Viewer;
    CHECK(noadm.auth().resolve("admin", &stored) && stored == Mesh485Role::Admin,
          "21 no-ADMIN file still has ADMIN");
    CHECK(!noadm.auth().lookup("eve", nullptr), "21 eve from bad file discarded");
  }
  {
    Mesh485AuthHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = Mesh485Auth::kMagic;
    h.version = Mesh485Auth::kVersion;
    h.count = MESH485_AUTH_CAP + 1;
    h.cap = MESH485_AUTH_CAP;
    uint8_t blob[sizeof(h)];
    memcpy(blob, &h, sizeof(h));
    mesh485_auth_poke_fake_fs(blob, sizeof(blob));
    Mesh485Dispatcher ovf;
    ovf.beginStore();
    CHECK(ovf.auth().count() == 3, "21 cap overflow → factory");
  }
  mesh485_auth_reset_fake_fs();
  {
    Mesh485Dispatcher missing;
    missing.beginStore();
    CHECK(missing.auth().count() == 3, "21 missing file → factory");
    Mesh485Role stored = Mesh485Role::Viewer;
    CHECK(missing.auth().resolve("admin", &stored) && stored == Mesh485Role::Admin,
          "21 missing file has ADMIN");
  }
  mesh485_auth_use_fake_fs(false);
  mesh485_auth_reset_fake_fs();

  // --- Phase 22 optional pubkey bind ---
  {
    uint8_t pub[MESH485_PUBKEY_LEN];
    CHECK(mesh485_parse_pubkey_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", pub),
          "22 parse good hex");
    CHECK(!mesh485_parse_pubkey_hex("zz", pub), "22 parse short");
    CHECK(!mesh485_parse_pubkey_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeX", pub),
          "22 parse bad nibble");
  }
  mesh485_auth_reset_fake_fs();
  mesh485_auth_use_fake_fs(true);
  {
    Mesh485AuthHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = Mesh485Auth::kMagic;
    h.version = Mesh485Auth::kVersionV1;
    h.count = 3;
    h.cap = MESH485_AUTH_CAP;
    Mesh485AuthRecord recs[MESH485_AUTH_CAP];
    memset(recs, 0, sizeof(recs));
    memcpy(recs[0].name, "viewer", 7);
    recs[0].role = (uint8_t)Mesh485Role::Viewer;
    memcpy(recs[1].name, "operator", 9);
    recs[1].role = (uint8_t)Mesh485Role::Operator;
    memcpy(recs[2].name, "admin", 6);
    recs[2].role = (uint8_t)Mesh485Role::Admin;
    uint8_t blob[sizeof(h) + sizeof(recs)];
    memcpy(blob, &h, sizeof(h));
    memcpy(blob + sizeof(h), recs, sizeof(recs));
    mesh485_auth_poke_fake_fs(blob, sizeof(blob));
    Mesh485Dispatcher v1;
    v1.beginStore();
    CHECK(v1.auth().count() == 3, "22 v1 blob factory count");
    CHECK(!v1.auth().hasPubkey("admin"), "22 v1 rows unbound");
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = v1.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0001\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 v1 admin OPEN unbound");
  }
  {
    Mesh485Dispatcher d22;
    d22.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0002\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"role\":\"operator\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 AUTH_SET lab");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0003\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"pubkey\":"
        "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 bind lab pubkey");
    CHECK(d22.auth().hasPubkey("lab"), "22 lab bound");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0004\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "22 bound OPEN no key");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0005\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"pubkey\":"
        "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 bound OPEN matching key");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0006\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"pubkey\":"
        "\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "22 bound OPEN wrong key");
    d22.clearReplay();
    Mesh485Env env_tx = makeEnv(&bus);
    CHECK(mesh485_parse_pubkey_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
              env_tx.sender_pub),
          "22 env sender parse");
    env_tx.sender_pub_set = true;
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0007\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env_tx, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 bound OPEN env.sender_pub");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0008\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "22 unbound VIEWER OPEN");
    d22.clearReplay();
    n = d22.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK0009\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"eve\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "22 eve still BAD_AUTH");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    CHECK(reboot.auth().hasPubkey("lab"), "22 bind survives persist");
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK000A\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "22 reboot bound no key");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK000B\",\"node\":\"EXT-001\","
        "\"op\":\"AUTH_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"name\":\"lab\",\"pubkey\":\"\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 unbind lab");
    CHECK(!reboot.auth().hasPubkey("lab"), "22 lab unbound");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK000C\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"lab\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "22 unbound lab OPEN");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PK000D\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "22 still RAW-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "22 still S20-001");
  }
  mesh485_auth_use_fake_fs(false);
  mesh485_auth_reset_fake_fs();

  // --- Phase 23 persist CONFIG protocol (SPIFFS blob / native fake-fs) ---
  mesh485_cfg_reset_fake_fs();
  mesh485_cfg_use_fake_fs(true);
  {
    Mesh485Dispatcher d23;
    d23.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0001\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"RAW\"") != nullptr, "23 default CONFIG_GET RAW");
    CHECK(strstr(out, "\"AUTO\"") == nullptr, "23 default no AUTO");
    CHECK(mesh485_cfg_fake_fs_len() == 0, "23 missing file not created on GET");
  }
  {
    Mesh485Dispatcher d23;
    d23.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0002\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"AUTO\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"UNKNOWN_CMD\"") != nullptr, "23 AUTO refused");
    CHECK(strstr(out, "no AUTO") != nullptr, "23 AUTO payload");
    CHECK(mesh485_cfg_fake_fs_len() == 0, "23 AUTO does not create file");
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0003\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "23 empty persist refused");
    CHECK(mesh485_cfg_fake_fs_len() == 0, "23 empty persist no file");
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0004\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "23 VIEWER CONFIG_SET");
    CHECK(mesh485_cfg_fake_fs_len() == 0, "23 VIEWER does not write file");
  }
  {
    Mesh485Dispatcher d23;
    d23.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0005\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "23 CONFIG_SET SIGNAL10");
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr, "23 echo SIGNAL10");
    CHECK(mesh485_cfg_fake_fs_len() == sizeof(Mesh485CfgBlob), "23 SIGNAL10 wrote blob");
    uint8_t snap[sizeof(Mesh485CfgBlob)];
    const size_t snap_n = mesh485_cfg_fake_fs_copy(snap, sizeof(snap));
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0006\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"AUTO\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "no AUTO") != nullptr, "23 AUTO after persist still refused");
    uint8_t after[sizeof(Mesh485CfgBlob)];
    CHECK(mesh485_cfg_fake_fs_copy(after, sizeof(after)) == snap_n, "23 AUTO same file size");
    CHECK(memcmp(snap, after, snap_n) == 0, "23 AUTO does not corrupt blob");
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0007\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr,
          "23 GET after AUTO still SIGNAL10");
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0008\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "23 factory admin OPEN");
    d23.clearReplay();
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0009\",\"node\":\"EXT-001\","
        "\"op\":\"ACCESS_OPEN\",\"operator\":\"eve\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"BAD_AUTH\"") != nullptr, "23 eve BAD_AUTH");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    CHECK(strcmp(reboot.devices().busProtocol(), "BOLID_SIGNAL10") == 0,
          "23 SIGNAL10 RAM after begin()");
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000A\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL10\"") != nullptr, "23 reboot SIGNAL10");
  }
  {
    Mesh485Dispatcher d23;
    d23.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = d23.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000B\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_C2000PP\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "23 CONFIG_SET C2000PP");
    CHECK(strstr(out, "\"protocol\":\"BOLID_C2000PP\"") != nullptr, "23 echo C2000PP");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000C\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"BOLID_C2000PP\"") != nullptr, "23 reboot C2000PP");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000D\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL20\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL20\"") != nullptr, "23 CONFIG_SET SIGNAL20");
  }
  {
    Mesh485Dispatcher reboot;
    reboot.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000E\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"BOLID_SIGNAL20\"") != nullptr, "23 reboot SIGNAL20");
    reboot.clearReplay();
    n = reboot.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF000F\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"viewer\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "23 still RAW-001");
    CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "23 still TS-001");
    CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "23 still PP-001");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "23 still S10-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "23 still S20-001");
  }
  {
    Mesh485CfgBlob junk;
    memset(&junk, 0xFF, sizeof(junk));
    mesh485_cfg_poke_fake_fs((const uint8_t*)&junk, sizeof(junk));
    Mesh485Dispatcher bad;
    bad.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = bad.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0010\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"RAW\"") != nullptr, "23 bad magic → RAW");
  }
  {
    Mesh485CfgBlob blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic = Mesh485BusCfg::kMagic;
    blob.version = Mesh485BusCfg::kVersion;
    memcpy(blob.protocol, "AUTO", 5);
    mesh485_cfg_poke_fake_fs((const uint8_t*)&blob, sizeof(blob));
    Mesh485Dispatcher auto_blob;
    auto_blob.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    n = auto_blob.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"CF0011\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_GET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"protocol\":\"RAW\"") != nullptr, "23 AUTO in blob → RAW");
  }
  {
    Mesh485CfgBlob blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic = Mesh485BusCfg::kMagic;
    blob.version = Mesh485BusCfg::kVersion;
    memcpy(blob.protocol, "FOO", 4);
    mesh485_cfg_poke_fake_fs((const uint8_t*)&blob, sizeof(blob));
    Mesh485Dispatcher unk;
    unk.beginStore();
    CHECK(strcmp(unk.devices().busProtocol(), "RAW") == 0, "23 unknown protocol → RAW");
  }
  mesh485_cfg_reset_fake_fs();
  {
    Mesh485Dispatcher missing;
    missing.beginStore();
    CHECK(strcmp(missing.devices().busProtocol(), "RAW") == 0, "23 missing file → RAW");
  }
  mesh485_cfg_use_fake_fs(false);
  mesh485_cfg_reset_fake_fs();

  // --- Phase 25 PCN dry contact (GPIO 47). Edges only. Not an Orion frame. ---
  mesh485_set_now_ms(0);
  {
    Mesh485Dispatcher d25;
    d25.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    CHECK(PcnContactAdapter::kPin == 47, "25 GPIO 47");
    CHECK(d25.devices().count() == 6, "25 six adapters");
    d25.devices().pcn().setRawClosed(false);
    d25.pollDevices();
    mesh485_set_now_ms(PcnContactAdapter::kDebounceMs);
    d25.pollDevices();
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN001\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"type\":\"PCN\"") == nullptr, "25 open start no PCN");
    CHECK(strstr(out, "\"state\":\"ALARM\"") == nullptr, "25 open start no ALARM");

    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN002\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "25 still RAW-001");
    CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "25 still TS-001");
    CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "25 still PP-001");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "25 still S10-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "25 still S20-001");
    CHECK(strstr(out, "\"id\":\"PCN-001\"") != nullptr, "25 DISCOVER PCN-001");
    CHECK(strstr(out, "\"protocol\":\"PCN_CONTACT\"") != nullptr, "25 protocol PCN_CONTACT");

    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN003\",\"node\":\"EXT-001\","
        "\"op\":\"NODE_STATUS\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"devices\":6") != nullptr, "25 devices 6");

    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN004\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_INFO\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PCN-001\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"capabilities\":[\"READ_STATUS\"]") != nullptr, "25 READ_STATUS only");
    CHECK(strstr(out, "CONTROL") == nullptr, "25 no CONTROL");

    const uint32_t t0 = 1000;
    d25.devices().pcn().setRawClosed(true);
    mesh485_set_now_ms(t0);
    d25.pollDevices();
    mesh485_set_now_ms(t0 + PcnContactAdapter::kDebounceMs - 1);
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN005\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"type\":\"PCN\"") == nullptr, "25 bounce under 50 ms no event");

    mesh485_set_now_ms(t0 + PcnContactAdapter::kDebounceMs);
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN006\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "25 one ALARM edge");
    CHECK(strstr(out, "\"device_id\":\"PCN-001\"") != nullptr, "25 device PCN-001");
    CHECK(strstr(out, "\"point_id\":\"ALARM\"") != nullptr, "25 point ALARM");
    CHECK(strstr(out, "\"type\":\"PCN\",\"severity\":\"ALARM\",\"state\":\"ALARM\"") != nullptr,
          "25 ALARM severity with ALARM state");
    CHECK(strstr(out, "\"source\":\"GPIO\"") != nullptr, "25 source GPIO");
    CHECK(strstr(out, "\"mesh_ok\":false") != nullptr, "25 mesh_ok 0");
    CHECK(strstr(out, "\"device_ok\":true") != nullptr, "25 device_ok 1");
    CHECK(strcmp(d25.devices().pcn().overlayState(), "ALARM") == 0, "25 overlay ALARM");

    mesh485_set_now_ms(t0 + 500);
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN007\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "25 hold no second ALARM");

    const uint32_t t1 = 2000;
    d25.devices().pcn().setRawClosed(false);
    mesh485_set_now_ms(t1);
    d25.pollDevices();
    mesh485_set_now_ms(t1 + PcnContactAdapter::kDebounceMs);
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN008\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 2, "25 release one NORMAL");
    CHECK(count_substr(out, "\"state\":\"ALARM\"") == 1, "25 still one ALARM");
    CHECK(strstr(out, "\"type\":\"PCN\",\"severity\":\"INFO\",\"state\":\"NORMAL\"") != nullptr,
          "25 NORMAL severity INFO");
    CHECK(strcmp(d25.devices().pcn().overlayState(), "NORMAL") == 0, "25 overlay NORMAL");
    const char* first_uuid = strstr(out, "00000000-0000-4000-8005-");
    const char* second_uuid =
        first_uuid ? strstr(first_uuid + 36, "00000000-0000-4000-8005-") : nullptr;
    CHECK(first_uuid != nullptr && second_uuid != nullptr, "25 two uuids");
    CHECK(first_uuid && second_uuid && strncmp(first_uuid, second_uuid, 36) != 0,
          "25 uuids differ");

    const uint32_t t2 = 3000;
    d25.devices().pcn().setRawClosed(true);
    mesh485_set_now_ms(t2);
    d25.pollDevices();
    d25.devices().pcn().setRawClosed(false);
    mesh485_set_now_ms(t2 + 20);
    d25.pollDevices();
    mesh485_set_now_ms(t2 + 200);
    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN009\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 2, "25 short glitch is not an edge");

    n = d25.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN00A\",\"node\":\"EXT-001\","
        "\"op\":\"COMMAND_EXECUTE\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"device_id\":\"PCN-001\",\"capability\":\"CONTROL_RELAY\","
        "\"point_id\":\"ALARM\",\"action\":\"ON\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"error\":\"NO_PERMISSION\"") != nullptr, "25 CONTROL refused");
  }
  {
    OrionReplyPort orion;
    Mesh485Dispatcher d_or;
    d_or.beginStore();
    Mesh485Env env = makeEnv(&orion);
    mesh485_set_now_ms(0);
    d_or.devices().pcn().setRawClosed(false);
    n = d_or.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN00B\",\"node\":\"EXT-001\","
        "\"op\":\"CONFIG_SET\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{\"protocol\":\"BOLID_SIGNAL10\"}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"result\":\"OK\"") != nullptr, "25 CONFIG_SET SIGNAL10");
    mesh485_set_now_ms(Signal10Adapter::kPollIntervalMs);
    n = d_or.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"PCN00C\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"state\":\"ALARM\"") == nullptr, "25 CRC frame is not ALARM");
    CHECK(strstr(out, "\"type\":\"PCN\"") == nullptr, "25 CRC frame is not PCN");
    CHECK(strstr(out, "FIRE") == nullptr, "25 CRC frame is not FIRE");
    CHECK(strstr(out, "\"type\":\"DIAG\"") != nullptr, "25 CRC frame can be DIAG");
  }
  mesh485_set_now_ms(0);

  // --- Phase 26 PCN hop (bytes, no air, not chat) ---
  {
    Mesh485Dispatcher panel;
    Mesh485Dispatcher monitor;
    panel.beginStore();
    monitor.beginStore();
    panel.setNodeId("EXT-001");
    monitor.setNodeId("EXT-002");
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    const uint32_t tx0 = bus.txCount();
    mesh485_set_now_ms(0);
    panel.devices().pcn().setRawClosed(false);
    panel.pollDevices();
    mesh485_set_now_ms(PcnContactAdapter::kDebounceMs);
    panel.pollDevices();

    panel.devices().pcn().setRawClosed(true);
    mesh485_set_now_ms(1000);
    panel.pollDevices();
    mesh485_set_now_ms(1000 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    CHECK(panel.devices().pcn().alarm(), "26 local contact alarm");
    CHECK(strcmp(panel.pcnShownState(), "ALARM") == 0, "26 panel overlay ALARM");

    uint8_t frame[80];
    const uint32_t t_send = 1100;
    size_t fn = panel.pollPcnHop(frame, sizeof(frame), t_send);
    CHECK(fn == kPcnHopBytes, "26 hop frame");
    CHECK(pcn_hop_is(frame, fn), "26 magic");
    CHECK(panel.pollPcnHop(frame, sizeof(frame), t_send + 500) == 0, "26 backoff holds");
    n = panel.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26001\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "26 silence keeps the row");
    CHECK(strstr(out, "\"mesh_ok\":false") != nullptr, "26 mesh_ok stays 0");
    CHECK(panel.devices().pcn().alarm(), "26 silence does not clear the input");

    uint8_t again[80];
    const size_t fn2 = panel.pollPcnHop(again, sizeof(again), t_send + 1000);
    CHECK(fn2 == kPcnHopBytes, "26 retry frame");
    CHECK(memcmp(frame + 10, again + 10, 36) == 0, "26 retry same uuid");

    uint8_t ack[80];
    size_t an = 0;
    CHECK(panel.acceptPcnFrame(again, fn2, ack, &an), "26 own echo accepted as noise");
    CHECK(an == 0, "26 own echo does not ack");
    CHECK(panel.events().count() == 1, "26 own echo does not duplicate");

    an = 0;
    CHECK(monitor.acceptPcnFrame(again, fn2, ack, &an), "26 monitor takes the frame");
    CHECK(an == kPcnHopBytes, "26 monitor ack");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "26 monitor overlay ALARM");
    CHECK(strcmp(monitor.gate().stateName(), "LOCKED") == 0, "26 hop does not open GPIO 40");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26002\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"state\":\"ALARM\"") == 1, "26 one ALARM on monitor");
    CHECK(strstr(out, "\"source\":\"LORA\"") != nullptr, "26 source LORA");
    CHECK(strstr(out, "\"type\":\"PCN\"") != nullptr, "26 type PCN");
    char alarm_uuid[40];
    memset(alarm_uuid, 0, sizeof(alarm_uuid));
    memcpy(alarm_uuid, again + 10, 36);

    an = 0;
    CHECK(monitor.acceptPcnFrame(again, fn2, ack, &an), "26 repeat frame");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26003\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "26 repeat does not double ALARM");

    CHECK(panel.acceptPcnFrame(ack, an, nullptr, nullptr), "26 panel takes ack");
    n = panel.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26004\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"mesh_ok\":true") != nullptr, "26 ack sets mesh_ok");
    CHECK(panel.pollPcnHop(frame, sizeof(frame), t_send + 5000) == 0, "26 acked hop is quiet");

    panel.devices().pcn().setRawClosed(false);
    mesh485_set_now_ms(2500);
    panel.pollDevices();
    mesh485_set_now_ms(2500 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    CHECK(!panel.devices().pcn().alarm(), "26 contact released");
    fn = panel.pollPcnHop(frame, sizeof(frame), 2600);
    CHECK(fn == kPcnHopBytes, "26 NORMAL frame");
    CHECK(memcmp(frame + 10, alarm_uuid, 36) != 0, "26 NORMAL is a new uuid");
    an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "26 monitor takes NORMAL");
    CHECK(panel.acceptPcnFrame(ack, an, nullptr, nullptr), "26 panel acks NORMAL");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26005\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"state\":\"ALARM\"") == 1, "26 still one ALARM");
    CHECK(count_substr(out, "\"state\":\"NORMAL\"") == 1, "26 one NORMAL");
    CHECK(strcmp(monitor.pcnShownState(), "NORMAL") == 0, "26 monitor overlay NORMAL");
    an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "26 repeat NORMAL");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"H26006\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 2, "26 repeat does not double the journal");
    CHECK(monitor.pollPcnHop(frame, sizeof(frame), 4000) == 0, "26 monitor does not retransmit");
    CHECK(strcmp(monitor.gate().stateName(), "LOCKED") == 0, "26 still locked");
    CHECK(bus.txCount() == tx0, "26 hop does not TX the panel bus");
    CHECK(strstr(out, "sendTxt") == nullptr, "26 not a chat payload");
  }
  mesh485_set_now_ms(0);

  // --- Phase 27 RDM6300 into the existing ACL (no seventh DISCOVER id) ---
  {
    const uint8_t raw[5] = {0xAB, 0xCD, 0xEF, 0x01, 0x23};
    uint8_t good[14];
    uint8_t badf[14];
    rdm_frame(good, raw, false);
    rdm_frame(badf, raw, true);
    Mesh485Dispatcher d27;
    d27.beginStore();
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    mesh485_set_now_ms(0);
    bool opened = false;
    CHECK(!d27.pushRdm(badf, 14, 0, "ENTRY", true, &opened), "27 bad checksum is not a card");
    CHECK(!opened && d27.rdmErrorCount() == 1, "27 bad checksum counts");
    CHECK(strcmp(d27.gate().stateName(), "LOCKED") == 0, "27 bad frame stays LOCKED");
    const uint8_t noise[3] = {0x00, 0xFF, 0x11};
    CHECK(!d27.pushRdm(noise, 3, 10, "ENTRY", true, &opened), "27 noise is not a card");
    CHECK(d27.rdmErrorCount() == 1, "27 noise is not a frame");

    CHECK(!d27.pushRdm(good, 7, 100, "ENTRY", true, &opened), "27 partial frame waits");
    CHECK(d27.pushRdm(good + 7, 7, 100, "ENTRY", true, &opened), "27 frame completes");
    CHECK(!opened, "27 empty ACL does not open");
    CHECK(strcmp(d27.gate().stateName(), "LOCKED") == 0, "27 DENY stays LOCKED");
    n = d27.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"R27001\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"device_id\":\"TS-001\"") != nullptr, "27 card is TS-001");
    CHECK(strstr(out, "\"source\":\"RFID\"") != nullptr, "27 source RFID");
    CHECK(strstr(out, "\"status\":\"Denied\"") != nullptr, "27 deny event");
    CHECK(strstr(out, "ABCDEF0123") != nullptr, "27 uid");
    CHECK(count_substr(out, "\"type\":\"CARD\"") == 1, "27 one deny row");

    CHECK(!d27.pushRdm(good, 14, 1100, "ENTRY", true, &opened), "27 repeat inside 2 s");
    n = d27.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"R27002\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"CARD\"") == 1, "27 repeat is one row");

    CHECK(d27.acl().addCard("ABCDEF0123", "E1", "Ann", true), "27 add card");
    opened = false;
    CHECK(d27.pushRdm(good, 14, 2600, "ENTRY", true, &opened), "27 listed card");
    CHECK(opened, "27 GRANT");
    CHECK(strcmp(d27.gate().stateName(), "OPEN") == 0, "27 gate OPEN");
    n = d27.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"R27003\",\"node\":\"EXT-001\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(strstr(out, "\"status\":\"Authorized\"") != nullptr, "27 authorized event");

    d27.setRdmIgnore(true);
    opened = false;
    CHECK(!d27.pushRdm(good, 14, 5000, "ENTRY", true, &opened), "27 ignore drops the read");
    CHECK(!opened, "27 ignore does not open");

    d27.setRdmMode(true);
    const uint32_t tx = bus.txCount();
    mesh485_set_now_ms(C2000ppModbusAdapter::kPollIntervalMs);
    n = d27.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"R27004\",\"node\":\"EXT-001\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(bus.txCount() == tx, "27 reader mode does not TX");
    CHECK(strstr(out, "\"id\":\"RAW-001\"") != nullptr, "27 still RAW-001");
    CHECK(strstr(out, "\"id\":\"TS-001\"") != nullptr, "27 still TS-001");
    CHECK(strstr(out, "\"id\":\"PP-001\"") != nullptr, "27 still PP-001");
    CHECK(strstr(out, "\"id\":\"S10-001\"") != nullptr, "27 still S10-001");
    CHECK(strstr(out, "\"id\":\"S20-001\"") != nullptr, "27 still S20-001");
    CHECK(strstr(out, "\"id\":\"PCN-001\"") != nullptr, "27 still PCN-001");
    CHECK(strstr(out, "\"id\":\"RDM") == nullptr, "27 no reader device id");

    Mesh485Dispatcher mon;
    mon.beginStore();
    mon.setNodeId("EXT-002");
    uint8_t hop[80];
    const size_t hn = pcn_hop_encode_state(hop, sizeof(hop), "EXT-001",
                                            "00000000-0000-4000-8005-000000000099", "ALARM", 9);
    uint8_t ack[80];
    size_t an = 0;
    CHECK(mon.acceptPcnFrame(hop, hn, ack, &an), "27 monitor takes PCN");
    CHECK(strcmp(mon.gate().stateName(), "LOCKED") == 0, "27 PCN hop does not open");
    an = 0;
    CHECK(mon.acceptPcnFrame(hop, hn, ack, &an), "27 PCN repeat");
    n = mon.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"R27005\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"state\":\"ALARM\"") == 1, "27 one PCN ALARM");
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "27 PCN repeat does not double");
  }
  mesh485_set_now_ms(0);

  // --- Phase 28 joint bench (bytes, no air, no live panel, no live reader) ---
  {
    Mesh485Dispatcher panel;
    Mesh485Dispatcher monitor;
    panel.beginStore();
    monitor.beginStore();
    panel.setNodeId("EXT-001");
    monitor.setNodeId("EXT-002");
    monitor.setRdmMode(true);
    EmptyBusPort bus;
    Mesh485Env env = makeEnv(&bus);
    const uint32_t tx0 = bus.txCount();
    mesh485_set_now_ms(0);
    panel.devices().pcn().setRawClosed(false);
    panel.pollDevices();
    mesh485_set_now_ms(PcnContactAdapter::kDebounceMs);
    panel.pollDevices();

    panel.devices().pcn().setRawClosed(true);
    mesh485_set_now_ms(1000);
    panel.pollDevices();
    mesh485_set_now_ms(1000 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    uint8_t frame[80];
    const size_t fn = panel.pollPcnHop(frame, sizeof(frame), 1100);
    CHECK(fn == kPcnHopBytes, "28 hop frame");
    uint8_t ack[80];
    size_t an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "28 monitor takes ALARM");
    CHECK(panel.acceptPcnFrame(ack, an, nullptr, nullptr), "28 panel takes ack");
    an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "28 repeat uuid");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "28 alarm stays on screen");
    CHECK(strcmp(monitor.gate().stateName(), "LOCKED") == 0, "28 contact does not pulse GPIO 40");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"J28001\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "28 one ALARM row");
    CHECK(strstr(out, "\"source\":\"LORA\"") != nullptr, "28 source LORA");

    const uint8_t listed[5] = {0xAB, 0xCD, 0xEF, 0x01, 0x23};
    const uint8_t stranger[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t good[14];
    uint8_t bad[14];
    rdm_frame(good, listed, false);
    rdm_frame(bad, stranger, false);
    bool opened = false;
    CHECK(monitor.pushRdm(bad, 14, 2000, "ENTRY", true, &opened), "28 unknown card");
    CHECK(!opened, "28 unknown card does not open");
    CHECK(strcmp(monitor.gate().stateName(), "LOCKED") == 0, "28 deny stays LOCKED");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "28 deny keeps the alarm");
    monitor.pollDevices();

    CHECK(monitor.acl().addCard("ABCDEF0123", "E1", "Ann", true), "28 add card");
    opened = false;
    CHECK(monitor.pushRdm(good, 14, 4500, "ENTRY", true, &opened), "28 listed card during alarm");
    CHECK(opened, "28 listed card opens");
    CHECK(strcmp(monitor.gate().stateName(), "OPEN") == 0, "28 gate OPEN while alarm");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "28 alarm still shown");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"J28002\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "28 card does not double PCN");
    CHECK(strstr(out, "\"device_id\":\"TS-001\"") != nullptr, "28 passage is TS-001");
    CHECK(strstr(out, "\"status\":\"Authorized\"") != nullptr, "28 authorized passage");
    CHECK(strstr(out, "\"status\":\"Denied\"") != nullptr, "28 deny row kept");

    panel.devices().pcn().setRawClosed(false);
    mesh485_set_now_ms(6000);
    panel.pollDevices();
    mesh485_set_now_ms(6000 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    uint8_t held[80];
    const size_t held_n = panel.pollPcnHop(held, sizeof(held), 6100);
    CHECK(held_n == kPcnHopBytes, "28 undelivered NORMAL is one frame");
    CHECK(panel.pollPcnHop(frame, sizeof(frame), 6100 + 500) == 0, "28 radio-off holds the retry");
    CHECK(monitor.acl().checkUid("ABCDEF0123").result == AccessResult::Allow,
          "28 card list works with radio quiet");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "28 quiet radio keeps the alarm");
    const size_t fn2 = panel.pollPcnHop(frame, sizeof(frame), 6100 + 1000);
    CHECK(fn2 == kPcnHopBytes, "28 retry is the same change");
    CHECK(memcmp(held + 10, frame + 10, 36) == 0, "28 retry keeps the uuid");
    an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn2, ack, &an), "28 radio returns the change");
    CHECK(panel.acceptPcnFrame(ack, an, nullptr, nullptr), "28 NORMAL acked");
    an = 0;
    CHECK(monitor.acceptPcnFrame(frame, fn2, ack, &an), "28 NORMAL repeat");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"J28003\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"state\":\"NORMAL\"") == 1, "28 one NORMAL, no duplicate");
    CHECK(count_substr(out, "\"state\":\"ALARM\"") == 1, "28 ALARM row remains");
    CHECK(strcmp(monitor.pcnShownState(), "NORMAL") == 0, "28 overlay follows the change");

    Mesh485Dispatcher panel2;
    panel2.beginStore();
    panel2.setNodeId("EXT-001");
    panel2.devices().pcn().setRawClosed(true);
    mesh485_set_now_ms(9000);
    panel2.pollDevices();
    mesh485_set_now_ms(9000 + PcnContactAdapter::kDebounceMs);
    panel2.pollDevices();
    CHECK(panel2.pollPcnHop(frame, sizeof(frame), 9100) == 0, "28 reboot on a held contact sends nothing");
    CHECK(strcmp(monitor.pcnShownState(), "NORMAL") == 0, "28 monitor keeps the last state");
    CHECK(monitor.acl().checkUid("ABCDEF0123").result == AccessResult::Allow,
          "28 card list survives the quiet radio");
    const uint32_t tx = bus.txCount();
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"J28004\",\"node\":\"EXT-002\","
        "\"op\":\"DEVICE_DISCOVER\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(bus.txCount() == tx && bus.txCount() == tx0, "28 reader mode and hop do not TX");
    CHECK(strstr(out, "\"id\":\"PCN-001\"") != nullptr, "28 still PCN-001");
    CHECK(strstr(out, "\"id\":\"RDM") == nullptr, "28 no seventh id");
    CHECK(strstr(out, "FIRE") == nullptr, "28 no invented fire");
  }
  mesh485_set_now_ms(0);

  // --- Two inputs, 15 s beacon, receiver holds 1 min after the last ALARM ---
  {
    CHECK(kPcnBeaconMs == 15000, "beacon is 15 s");
    CHECK(kPcnHoldMs == 60000, "receiver hold is 1 min");
    Mesh485Dispatcher panel;
    Mesh485Dispatcher monitor;
    panel.beginStore();
    monitor.beginStore();
    panel.setNodeId("EXT-001");
    monitor.setNodeId("EXT-002");
    monitor.setRoleSource(false);
    mesh485_set_now_ms(0);
    panel.devices().pcn().setRawClosed(false);
    panel.devices().pcn().setRawClosed2(false);
    panel.pollDevices();
    mesh485_set_now_ms(PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    panel.devices().pcn().setRawClosed2(true);
    mesh485_set_now_ms(1000);
    panel.pollDevices();
    mesh485_set_now_ms(1000 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    CHECK(panel.devices().pcn().alarm(), "GPIO 45 alone is alarm");
    CHECK(panel.devices().pcn().episodeAlarm(), "GPIO 45 edge is an episode");

    uint8_t frame[80];
    const uint32_t t0 = 1100;
    size_t fn = panel.pollPcnHop(frame, sizeof(frame), t0);
    CHECK(fn == kPcnHopBytes, "edge frame");
    CHECK(frame[54] == 1, "hop channel byte is 1");
    CHECK(panel.pollPcnHop(frame, sizeof(frame), t0 + 500) == 0, "backoff before beacon");
    uint8_t ack[80];
    size_t an = 0;
    mesh485_set_now_ms(t0);
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "receiver takes ALARM");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "receiver shows ALARM");
    PcnScreen shown;
    monitor.fillAlarm(&shown);
    CHECK(strstr(shown.line_a, "Ульянова 1") != nullptr, "EXT-001 is Ульянова 1");
    CHECK(shown.active == 1, "one live alarm");
    CHECK(shown.channel == 1, "default channel 1");
    CHECK(shown.hist_n == 1, "history records the trigger");
    CHECK(panel.acceptPcnFrame(ack, an, nullptr, nullptr), "source takes ack");
    monitor.bumpChannel();
    CHECK(monitor.channel() == 2, "channel steps to 2");
    uint8_t ack_ignored[80];
    size_t an_ignored = 99;
    CHECK(monitor.acceptPcnFrame(frame, fn, ack_ignored, &an_ignored), "other channel is ignored");
    CHECK(an_ignored == 0, "other channel is not acked");
    monitor.fillAlarm(&shown);
    CHECK(shown.active == 1, "other channel does not add an alarm");
    for (uint8_t i = 0; i < 29; ++i) monitor.bumpChannel();
    CHECK(monitor.channel() == 1, "channel wraps after 30");
    CHECK(panel.pollPcnHop(frame, sizeof(frame), t0 + 5000) == 0, "quiet until 15 s");
    fn = panel.pollPcnHop(frame, sizeof(frame), t0 + kPcnBeaconMs);
    CHECK(fn == kPcnHopBytes, "beacon at 15 s");
    PcnHopView view;
    CHECK(pcn_hop_decode(frame, fn, &view), "beacon decodes");
    CHECK(strcmp(view.state, "ALARM") == 0, "beacon is ALARM");
    mesh485_set_now_ms(t0 + kPcnBeaconMs);
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "repeat beacon");
    n = monitor.handleLine(
        "{\"v\":1,\"type\":\"m485.request\",\"id\":\"B15001\",\"node\":\"EXT-002\","
        "\"op\":\"EVENT_LIST\",\"operator\":\"admin\",\"ts\":1787912345,\"nonce\":\"x\","
        "\"payload\":{}}",
        env, out, sizeof(out));
    CHECK(count_substr(out, "\"type\":\"PCN\"") == 1, "beacon does not add a row");

    panel.devices().pcn().setRawClosed2(false);
    mesh485_set_now_ms(20000);
    panel.pollDevices();
    mesh485_set_now_ms(20000 + PcnContactAdapter::kDebounceMs);
    panel.pollDevices();
    fn = panel.pollPcnHop(frame, sizeof(frame), 20100);
    CHECK(fn == kPcnHopBytes, "release sends NORMAL");
    mesh485_set_now_ms(20100);
    CHECK(monitor.acceptPcnFrame(frame, fn, ack, &an), "receiver stores NORMAL");
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "NORMAL does not clear the hold");
    mesh485_set_now_ms(t0 + kPcnHoldMs - 1);
    CHECK(strcmp(monitor.pcnShownState(), "ALARM") == 0, "hold lasts until 1 min");
    mesh485_set_now_ms(t0 + kPcnBeaconMs + kPcnHoldMs);
    CHECK(strcmp(monitor.pcnShownState(), "NORMAL") == 0, "1 min of silence clears");
  }
  mesh485_set_now_ms(0);

  // --- V3 alarm screen: 40 s after a new signal, 40 strobes, off when the signal is gone ---
  {
    CHECK(kAlarmHoldMs == 40000, "alarm hold is 40 s");
    CHECK(kAlarmStrobeCount == 40, "alarm strobe is 40 flashes");
    AlarmUiState st = {0, 0, 0};
    AlarmUiStep s = alarm_ui_step(&st, 1, 0, 1000, kAlarmHoldMs);
    CHECK(!s.start_strobe && !s.show && !st.showing, "stuck gen 0 does not light the screen");
    s = alarm_ui_step(&st, 1, 1, 5000, kAlarmHoldMs);
    CHECK(s.start_strobe && s.show && st.showing, "new alarm starts the strobe");
    CHECK(st.until == 5000 + kAlarmHoldMs, "display stays up for 40 s");
    s = alarm_ui_step(&st, 1, 1, 5000 + 10000, kAlarmHoldMs);
    CHECK(s.show && !s.start_strobe && !s.clear, "same signal keeps the screen");
    s = alarm_ui_step(&st, 0, 1, 5000 + 15000, kAlarmHoldMs);
    CHECK(s.clear && !st.showing, "no signal turns the alarm off");
    s = alarm_ui_step(&st, 1, 2, 30000, kAlarmHoldMs);
    CHECK(s.start_strobe && st.until == 30000 + kAlarmHoldMs, "next alarm is a new 40 s");
    s = alarm_ui_step(&st, 1, 2, 30000 + kAlarmHoldMs, kAlarmHoldMs);
    CHECK(s.clear && !st.showing, "40 s with no new signal turns the alarm off");
    s = alarm_ui_step(&st, 1, 2, 30000 + kAlarmHoldMs + 1000, kAlarmHoldMs);
    CHECK(!s.show && !s.start_strobe && !st.showing, "expired alarm does not stick");
  }

  run_orion_crc_tests();

  if (g_fail) {
    printf("%d test(s) failed\n", g_fail);
    return 1;
  }
  printf("native_protocol_tests: ok\n");
  return 0;
}
