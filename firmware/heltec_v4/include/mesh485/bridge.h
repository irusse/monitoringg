#pragma once

#include "mesh485/port.h"
#include "mesh485/types.h"

#include <stddef.h>
#include <stdint.h>

#ifndef MESH485_BRIDGE_IDLE_DEFAULT_S
#define MESH485_BRIDGE_IDLE_DEFAULT_S 30
#endif
#ifndef MESH485_BRIDGE_IDLE_MAX_S
#define MESH485_BRIDGE_IDLE_MAX_S 300
#endif
#ifndef MESH485_BRIDGE_RING_SLOTS
#define MESH485_BRIDGE_RING_SLOTS 8
#endif
#ifndef MESH485_BRIDGE_SLOT_MAX
#define MESH485_BRIDGE_SLOT_MAX 256
#endif

#ifndef MESH485_ALLOW_BRIDGE_CONTROL
#define MESH485_ALLOW_BRIDGE_CONTROL 0
#endif

enum class Mesh485BridgeLevel : uint8_t {
  Off = 0,
  ReadOnly = 1,
  Control = 2,
  Raw = 3,
  Unknown = 255,
};

struct Mesh485LastSession {
  bool used = false;
  char optr[24] = {};
  int64_t ts = 0;
  char level[16] = {};
};

// One RAM session per isolated UART. Authz is an ADMIN stub (prompt 14).
class Mesh485Bridge {
public:
  static Mesh485BridgeLevel parseLevel(const char* s);
  static const char* levelName(Mesh485BridgeLevel lv);

  bool isOpen() const { return open_; }
  Mesh485BridgeLevel level() const { return open_ ? level_ : Mesh485BridgeLevel::Off; }
  const char* levelName() const { return levelName(level()); }
  uint32_t idleS() const { return idle_s_; }
  uint32_t idleRemaining(uint32_t now_s) const;
  const Mesh485LastSession& lastSession() const { return last_; }

  // DE fail-safe RX on open and every close path.
  bool open(Mesh485BridgeLevel lv, uint32_t idle_s, const char* optr, int64_t ts,
            uint32_t now_s, Mesh485Port* port);
  void close();
  void tick(uint32_t now_s);
  void touch(uint32_t now_s);
  bool allowTransaction() const { return open_ && level_ == Mesh485BridgeLevel::Raw; }

  void capture(const uint8_t* data, size_t len, int64_t ts);
  // Drain into payload JSON. Returns true (always OK to poll; empty frames is OK).
  void drainPayload(char* out, size_t out_max);

private:
  bool open_ = false;
  Mesh485BridgeLevel level_ = Mesh485BridgeLevel::Off;
  uint32_t idle_s_ = MESH485_BRIDGE_IDLE_DEFAULT_S;
  uint32_t last_s_ = 0;
  Mesh485Port* port_ = nullptr;
  Mesh485LastSession last_{};

  struct Slot {
    uint8_t data[MESH485_BRIDGE_SLOT_MAX];
    uint16_t len;
    int64_t ts;
  };
  Slot ring_[MESH485_BRIDGE_RING_SLOTS] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint32_t dropped_ = 0;

  void failSafeRx();
  void clearRing();
};
