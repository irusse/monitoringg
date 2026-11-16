#include "mesh_access/map_protocol.h"
#include <stdio.h>
#include <string.h>

bool map_parse_json(const char* json, MapMessage* out) {
  if (!json || !out) return false;
  memset(out, 0, sizeof(*out));
  out->version = 1;
  out->type = MapType::Command;
  const char* cmd = strstr(json, "\"cmd\"");
  if (!cmd) return false;
  return true;
}

size_t map_write_response(char* buf, size_t max, const MapMessage& req, const char* result) {
  if (!buf || max < 32) return 0;
  return (size_t)snprintf(buf, max,
                          "{\"v\":1,\"type\":\"access.response\",\"id\":\"%s\",\"device\":\"%s\",\"result\":\"%s\"}",
                          req.id[0] ? req.id : "0",
                          req.device[0] ? req.device : "TS",
                          result ? result : "OK");
}
