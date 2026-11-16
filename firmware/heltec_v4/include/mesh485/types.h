#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef MESH485_JSON_MAX
#define MESH485_JSON_MAX 512
#endif
#ifndef MESH485_RESP_MAX
#define MESH485_RESP_MAX 2048
#endif
#ifndef MESH485_FRAME_MAX
#define MESH485_FRAME_MAX 256
#endif
#ifndef MESH485_B64_MAX
#define MESH485_B64_MAX 360
#endif
#ifndef MESH485_NODE_ID
#define MESH485_NODE_ID "EXT-001"
#endif
#ifndef MESH485_TXN_TIMEOUT_MAX_MS
#define MESH485_TXN_TIMEOUT_MAX_MS 2000
#endif
#ifndef MESH485_TXN_TIMEOUT_DEFAULT_MS
#define MESH485_TXN_TIMEOUT_DEFAULT_MS 900
#endif

enum class Mesh485Type : uint8_t { Request = 1, Response = 2, Event = 3, Unknown = 0 };

struct Mesh485Request {
  uint8_t v = 0;
  Mesh485Type type = Mesh485Type::Unknown;
  char id[12] = {};
  char node[16] = {};
  char op[24] = {};
  char optr[24] = {};
  int64_t ts = 0;
  char nonce[16] = {};
  char protocol[16] = {};
  uint32_t timeout_ms = MESH485_TXN_TIMEOUT_DEFAULT_MS;
  char frame_b64[MESH485_B64_MAX] = {};
  char level[16] = {};       // BRIDGE_OPEN; empty = READ_ONLY
  uint32_t idle_s = 0;       // BRIDGE_OPEN; 0 = default 30
  char device_id[16] = {};   // DEVICE_* / POINT_* / EVENT_LIST / COMMAND_EXECUTE
  char point_id[16] = {};    // POINT_STATUS / COMMAND_EXECUTE named point
  char since[40] = {};       // EVENT_LIST; seq or uuid
  char stub[16] = {};        // EVENT_SUBSCRIBE lab inject (CARD)
  char capability[24] = {};  // COMMAND_EXECUTE; must be on Device.capabilities
  char action[8] = {};       // COMMAND_EXECUTE ON | OFF
  char uid[24] = {};         // ACCESS_ADD_CARD / ACCESS_REMOVE_CARD
  char employee_id[32] = {};
  char card_name[32] = {};
  uint16_t pulse_ms = 300;   // ACCESS_OPEN
  uint8_t uid_enabled = 1;
  uint8_t uid_enabled_set = 0;
  uint16_t list_offset = 0;  // ACCESS_GET_USERS
  uint16_t list_limit = 0;   // ACCESS_GET_USERS; 0 = default 16
  char role[16] = {};        // AUTH_SET payload role (viewer|operator|admin)
  char pubkey[65] = {};      // payload.pubkey — 64 hex; presented key or AUTH_SET bind
  uint8_t pubkey_set = 0;    // 1 if payload had pubkey (string or null)
};

struct Mesh485Response {
  char id[12] = {};
  char node[16] = {};
  char op[24] = {};
  char result[16] = {};
  char error[24] = {};  // empty means JSON null
  int64_t ts = 0;
  char payload[1792] = {};  // already-serialized object body, including braces
};
