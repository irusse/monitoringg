#include "mesh485/pcn_hop.h"

#include <string.h>

static void put_str(char* dst, size_t max, const char* src) {
  memset(dst, 0, max);
  if (!src) return;
  strncpy(dst, src, max - 1);
}

bool pcn_hop_is(const uint8_t* frame, size_t len) {
  if (!frame || len < kPcnHopBytes || frame[0] != kPcnHopMagic) return false;
  if (frame[1] != kPcnHopState && frame[1] != kPcnHopAck) return false;
  if (len == kPcnHopBytes) return true;
  const uint8_t n = frame[kPcnHopBytes];
  return n > 0 && n <= kPcnHopNameMax && len == kPcnHopBytes + 1 + n;
}

static size_t encode(uint8_t* out, size_t max, uint8_t kind, const char* node, const char* uuid,
                      const char* state, uint32_t sequence, uint8_t channel, const char* label) {
  if (!out || max < kPcnHopBytes || !uuid || !uuid[0]) return 0;
  if (kind == kPcnHopState && (!state || !state[0])) return 0;
  memset(out, 0, kPcnHopBytes);
  out[0] = kPcnHopMagic;
  out[1] = kind;
  put_str((char*)out + 2, 8, node ? node : "");
  put_str((char*)out + 10, 37, uuid);
  put_str((char*)out + 47, 8, state ? state : "");
  if (channel < 1 || channel > kPcnChannelMax) channel = 1;
  out[54] = channel;
  memcpy(out + 55, &sequence, 4);
  size_t nlen = 0;
  if (kind == kPcnHopState && label && label[0]) {
    nlen = strlen(label);
    if (nlen > kPcnHopNameMax) nlen = kPcnHopNameMax;
    if (max < kPcnHopBytes + 1 + nlen) nlen = 0;
  }
  if (nlen) {
    out[kPcnHopBytes] = (uint8_t)nlen;
    memcpy(out + kPcnHopBytes + 1, label, nlen);
    return kPcnHopBytes + 1 + nlen;
  }
  return kPcnHopBytes;
}

size_t pcn_hop_encode_state(uint8_t* out, size_t max, const char* node, const char* uuid,
                            const char* state, uint32_t sequence, uint8_t channel,
                            const char* label) {
  return encode(out, max, kPcnHopState, node, uuid, state, sequence, channel, label);
}

size_t pcn_hop_encode_ack(uint8_t* out, size_t max, const char* uuid) {
  return encode(out, max, kPcnHopAck, "", uuid, "", 0, 1, nullptr);
}

bool pcn_hop_decode(const uint8_t* frame, size_t len, PcnHopView* out) {
  if (!pcn_hop_is(frame, len) || !out) return false;
  memset(out, 0, sizeof(*out));
  out->kind = frame[1];
  out->channel = frame[54];
  if (out->channel < 1 || out->channel > kPcnChannelMax) out->channel = 1;
  memcpy(out->node, frame + 2, 7);
  if (len > kPcnHopBytes) {
    const uint8_t n = frame[kPcnHopBytes];
    memcpy(out->label, frame + kPcnHopBytes + 1, n);
    out->label[n] = 0;
  }
  memcpy(out->uuid, frame + 10, 36);
  memcpy(out->state, frame + 47, 7);
  memcpy(&out->sequence, frame + 55, 4);
  return out->uuid[0] != 0;
}
