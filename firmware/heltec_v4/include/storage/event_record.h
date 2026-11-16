#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef EVENT_STORE_CAP
#define EVENT_STORE_CAP 32
#endif

// Packed so SPIFFS / native fake-FS blobs stay stable across reboot.
#pragma pack(push, 1)
struct EventRecord {
  char uuid[40];
  uint32_t sequence;
  char date[12];
  char time[10];
  char zone_id[16];
  char user_id[32];
  char name[32];
  char rfid_uid[24];
  char action[16];
  char status[16];
  char remarks[40];
  char device_id[16];
  char point_id[16];
  char type[16];
  char severity[12];
  char state[16];
  int64_t timestamp;
  char source[16];  // RFID | BLE | USB | LORA | RS485 | GPIO
  uint8_t mesh_ok;
  uint8_t device_ok;
  uint8_t mesh_retries;
  uint8_t pad;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct EventStoreHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint32_t next_seq;
  uint16_t head;
  uint16_t cap;
};
#pragma pack(pop)

void event_record_clear(EventRecord* e);
void event_record_copy_str(char* dst, size_t max, const char* src);
void event_record_fill_clock(EventRecord* e);
size_t event_record_json(const EventRecord& e, char* dst, size_t max);
size_t event_record_ndjson(const EventRecord& e, const char* node, char* dst, size_t max);
