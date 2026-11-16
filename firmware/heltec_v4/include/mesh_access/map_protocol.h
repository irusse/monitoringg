#pragma once

#include <stddef.h>
#include <stdint.h>

enum class MapType : uint8_t { Command = 1, Response = 2, Event = 3 };

struct MapMessage {
  uint8_t version;
  MapType type;
  char id[12];
  char device[16];
  char cmd[24];
  char result[16];
};

bool map_parse_json(const char* json, MapMessage* out);
size_t map_write_response(char* buf, size_t max, const MapMessage& req, const char* result);
