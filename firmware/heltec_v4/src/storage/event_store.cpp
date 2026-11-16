#include "storage/event_store.h"

#include <stdio.h>
#include <string.h>

#ifndef MESH485_NATIVE
#include <Arduino.h>
#include <SPIFFS.h>
static const char kPath[] = "/m485_evt.bin";
#endif

#ifdef MESH485_NATIVE
static bool g_fake_fs_on = false;
static uint8_t g_fake_fs[sizeof(EventStoreHeader) + EVENT_STORE_CAP * sizeof(EventRecord)];
static size_t g_fake_fs_len = 0;
#endif

void event_store_use_fake_fs(bool on) {
#ifdef MESH485_NATIVE
  g_fake_fs_on = on;
#else
  (void)on;
#endif
}

void event_store_reset_fake_fs() {
#ifdef MESH485_NATIVE
  g_fake_fs_len = 0;
  memset(g_fake_fs, 0, sizeof(g_fake_fs));
#endif
}

void EventStore::resetRam() {
  memset(recs_, 0, sizeof(recs_));
  head_ = 0;
  count_ = 0;
  next_seq_ = 1;
}

size_t EventStore::slot(size_t oldest_index) const {
  return (size_t)((head_ + (uint16_t)oldest_index) % kCap);
}

void EventStore::begin() {
  resetRam();
  ready_ = true;
  if (!load()) persist();
}

bool EventStore::hasUuid(const char* uuid) const {
  if (!uuid || !uuid[0]) return false;
  for (uint16_t i = 0; i < count_; ++i) {
    if (strcmp(recs_[slot(i)].uuid, uuid) == 0) return true;
  }
  return false;
}

int EventStore::pendingCount() const {
  int n = 0;
  for (uint16_t i = 0; i < count_; ++i) {
    if (!recs_[slot(i)].mesh_ok) ++n;
  }
  return n;
}

bool EventStore::getAt(size_t oldest_index, EventRecord* out) const {
  if (!out || oldest_index >= count_) return false;
  *out = recs_[slot(oldest_index)];
  return true;
}

bool EventStore::nextPendingMesh(EventRecord* out) const {
  if (!out) return false;
  for (uint16_t i = 0; i < count_; ++i) {
    const EventRecord& e = recs_[slot(i)];
    if (!e.mesh_ok && e.mesh_retries < kMaxMeshRetries) {
      *out = e;
      return true;
    }
  }
  return false;
}

bool EventStore::lastFlags(bool* mesh_ok, bool* device_ok) const {
  if (count_ == 0) return false;
  const EventRecord& e = recs_[slot(count_ - 1)];
  if (mesh_ok) *mesh_ok = e.mesh_ok != 0;
  if (device_ok) *device_ok = e.device_ok != 0;
  return true;
}

bool EventStore::append(EventRecord e, bool keep_sequence) {
  if (!ready_) begin();
  event_record_fill_clock(&e);
  if (!e.uuid[0]) {
    snprintf(e.uuid, sizeof(e.uuid), "00000000-0000-4000-8000-%012lu", (unsigned long)next_seq_);
  }
  if (hasUuid(e.uuid)) return true;
  if (keep_sequence && e.sequence > 0) {
    if (e.sequence >= next_seq_) next_seq_ = e.sequence + 1;
  } else {
    e.sequence = next_seq_++;
  }
  e.mesh_retries = 0;
  if (!e.source[0]) event_record_copy_str(e.source, sizeof(e.source), "USB");
  if (count_ == kCap) {
    head_ = (uint16_t)((head_ + 1) % kCap);
    --count_;
  }
  recs_[slot(count_)] = e;
  ++count_;
  return persist();
}

bool EventStore::ackMesh(uint32_t sequence) {
  for (uint16_t i = 0; i < count_; ++i) {
    EventRecord& e = recs_[slot(i)];
    if (e.sequence == sequence) {
      e.mesh_ok = 1;
      return persist();
    }
  }
  return false;
}

bool EventStore::failMesh(uint32_t sequence) {
  for (uint16_t i = 0; i < count_; ++i) {
    EventRecord& e = recs_[slot(i)];
    if (e.sequence == sequence) {
      if (e.mesh_retries < 255) ++e.mesh_retries;
      return persist();
    }
  }
  return false;
}

bool EventStore::persist() {
#ifdef MESH485_NATIVE
  if (!g_fake_fs_on) return true;
  EventStoreHeader h;
  h.magic = kMagic;
  h.version = 1;
  h.count = count_;
  h.next_seq = next_seq_;
  h.head = head_;
  h.cap = (uint16_t)kCap;
  memcpy(g_fake_fs, &h, sizeof(h));
  memcpy(g_fake_fs + sizeof(h), recs_, sizeof(recs_));
  g_fake_fs_len = sizeof(h) + sizeof(recs_);
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  File f = SPIFFS.open(kPath, "w");
  if (!f) return false;
  EventStoreHeader h;
  h.magic = kMagic;
  h.version = 1;
  h.count = count_;
  h.next_seq = next_seq_;
  h.head = head_;
  h.cap = (uint16_t)kCap;
  const size_t wh = f.write((const uint8_t*)&h, sizeof(h));
  const size_t wr = f.write((const uint8_t*)recs_, sizeof(recs_));
  f.close();
  return wh == sizeof(h) && wr == sizeof(recs_);
#endif
}

bool EventStore::load() {
#ifdef MESH485_NATIVE
  if (!g_fake_fs_on || g_fake_fs_len < sizeof(EventStoreHeader)) return false;
  EventStoreHeader h;
  memcpy(&h, g_fake_fs, sizeof(h));
  if (h.magic != kMagic || h.version != 1 || h.cap != kCap) return false;
  if (h.count > kCap) return false;
  memcpy(recs_, g_fake_fs + sizeof(h), sizeof(recs_));
  head_ = h.head;
  count_ = h.count;
  next_seq_ = h.next_seq ? h.next_seq : 1;
  return true;
#else
  if (!SPIFFS.begin(true)) return false;
  if (!SPIFFS.exists(kPath)) return false;
  File f = SPIFFS.open(kPath, "r");
  if (!f) return false;
  EventStoreHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) {
    f.close();
    return false;
  }
  if (h.magic != kMagic || h.version != 1 || h.cap != kCap || h.count > kCap) {
    f.close();
    return false;
  }
  if (f.read((uint8_t*)recs_, sizeof(recs_)) != sizeof(recs_)) {
    f.close();
    return false;
  }
  f.close();
  head_ = h.head;
  count_ = h.count;
  next_seq_ = h.next_seq ? h.next_seq : 1;
  return true;
#endif
}
