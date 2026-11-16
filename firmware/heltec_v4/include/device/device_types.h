#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef DEVICE_ID_MAX
#define DEVICE_ID_MAX 16
#endif
#ifndef DEVICE_CAP_MAX
#define DEVICE_CAP_MAX 4
#endif
#ifndef DEVICE_CAP_LEN
#define DEVICE_CAP_LEN 16
#endif
#ifndef DEVICE_POINT_ID_MAX
#define DEVICE_POINT_ID_MAX 16
#endif
#ifndef DEVICE_EVENT_ID_MAX
#define DEVICE_EVENT_ID_MAX 40
#endif

inline void device_copy(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

struct DeviceInfo {
  char id[DEVICE_ID_MAX] = {};
  char vendor[16] = {};
  char model[16] = {};
  char protocol[16] = {};
  uint16_t address = 0;
  bool online = false;
  char capabilities[DEVICE_CAP_MAX][DEVICE_CAP_LEN] = {};
  uint8_t cap_count = 0;
};

inline bool device_has_cap(const DeviceInfo& d, const char* cap) {
  if (!cap || !cap[0]) return false;
  for (uint8_t i = 0; i < d.cap_count && i < DEVICE_CAP_MAX; ++i) {
    if (strcmp(d.capabilities[i], cap) == 0) return true;
  }
  return false;
}

// COMMAND_EXECUTE body. `capability` must match Device.capabilities.
struct CommandRequest {
  char capability[DEVICE_CAP_LEN] = {};
  char point_id[DEVICE_POINT_ID_MAX] = {};
  char action[8] = {};  // ON | OFF
};

struct CommandOutcome {
  bool mesh_ok = false;
  bool device_ok = false;
  char error[24] = {};  // empty = success
};

struct DevicePoint {
  char device_id[DEVICE_ID_MAX] = {};
  char point_id[DEVICE_POINT_ID_MAX] = {};
  char type[16] = {};
  char name[24] = {};
  char state[16] = {};
};

struct DeviceEvent {
  char id[DEVICE_EVENT_ID_MAX] = {};
  char device_id[DEVICE_ID_MAX] = {};
  char point_id[DEVICE_POINT_ID_MAX] = {};
  char type[16] = {};
  char severity[16] = {};
  char state[16] = {};
  int64_t timestamp = 0;
  uint32_t sequence = 0;
  char source[16] = {};
  char rfid_uid[24] = {};
  char action[16] = {};
  char status[16] = {};
  char name[32] = {};
  bool mesh_ok = false;
  bool device_ok = false;
};
