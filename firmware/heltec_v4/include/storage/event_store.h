#pragma once

#include "storage/event_record.h"

// Persistent ring of MESH 485 / HAPRS-like events. Photos are never stored.
// Device: SPIFFS `/m485_evt.bin` (same FS as MeshCore DataStore — not LittleFS).
// Native: RAM only unless event_store_use_fake_fs(true) for reboot tests.

void event_store_use_fake_fs(bool on);
void event_store_reset_fake_fs();

class EventStore {
public:
  static constexpr size_t kCap = EVENT_STORE_CAP;
  static constexpr uint8_t kMaxMeshRetries = 8;
  static constexpr uint32_t kMagic = 0x45313551u;  // 'E11Q'

  void begin();
  // keep_sequence: hop ingest stores the sender sequence instead of minting one.
  bool append(EventRecord e, bool keep_sequence = false);
  int count() const { return (int)count_; }
  int pendingCount() const;
  bool hasUuid(const char* uuid) const;
  bool getAt(size_t oldest_index, EventRecord* out) const;
  bool nextPendingMesh(EventRecord* out) const;
  bool ackMesh(uint32_t sequence);
  bool failMesh(uint32_t sequence);
  bool lastFlags(bool* mesh_ok, bool* device_ok) const;

private:
  EventRecord recs_[kCap];
  uint16_t head_ = 0;
  uint16_t count_ = 0;
  uint32_t next_seq_ = 1;
  bool ready_ = false;

  size_t slot(size_t oldest_index) const;
  bool persist();
  bool load();
  void resetRam();
};
