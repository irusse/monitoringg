#pragma once

#include <stddef.h>
#include <stdint.h>

// Compact hop for one PCN state change. Not a MeshCore text message.
// Wire size is fixed and well under the MeshCore payload ceiling (184).

static constexpr uint8_t kPcnHopMagic = 0xA5;
static constexpr uint8_t kPcnHopState = 1;
static constexpr uint8_t kPcnHopAck = 2;
static constexpr size_t kPcnHopBytes = 59;
// Optional name after the fixed frame: 1 length byte + UTF-8, not part of the
// 59-byte core. Empty name keeps the frame at 59 bytes.
static constexpr size_t kPcnHopNameMax = 31;
// Source repeats ALARM on this period while the contact stays closed.
static constexpr uint32_t kPcnBeaconMs = 15000;
// Receiver keeps the alarm screen until no ALARM frame for this long.
static constexpr uint32_t kPcnHoldMs = 60000;
static constexpr uint8_t kPcnChannelMax = 30;

struct PcnHopView {
  uint8_t kind = 0;
  uint8_t channel = 1;
  char node[8] = {};
  char label[kPcnHopNameMax + 1] = {};
  char uuid[40] = {};
  char state[8] = {};
  uint32_t sequence = 0;
};

// Lines for the СИГНАЛИЗАЦИЯ screen and the last triggers.
struct PcnScreen {
  bool source = false;
  uint8_t channel = 1;
  uint8_t active = 0;
  char line_a[48] = {};
  char line_b[48] = {};
  char hist[3][32] = {};
  uint8_t hist_n = 0;
};

bool pcn_hop_is(const uint8_t* frame, size_t len);
// channel 0 is stored as 1. Byte 54 of the 59-byte frame, clear of the uuid.
size_t pcn_hop_encode_state(uint8_t* out, size_t max, const char* node, const char* uuid,
                            const char* state, uint32_t sequence, uint8_t channel = 1,
                            const char* label = nullptr);
size_t pcn_hop_encode_ack(uint8_t* out, size_t max, const char* uuid);
bool pcn_hop_decode(const uint8_t* frame, size_t len, PcnHopView* out);
