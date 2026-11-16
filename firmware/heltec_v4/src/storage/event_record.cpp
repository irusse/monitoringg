#include "storage/event_record.h"

#include "mesh485/types.h"

#include <stdio.h>
#include <string.h>

void event_record_copy_str(char* dst, size_t max, const char* src) {
  if (!dst || max == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, max - 1);
  dst[max - 1] = 0;
}

void event_record_clear(EventRecord* e) {
  if (!e) return;
  memset(e, 0, sizeof(*e));
}

void event_record_fill_clock(EventRecord* e) {
  if (!e) return;
  if (!e->time[0]) {
    uint32_t s = 0;
    if (e->timestamp > 0) s = (uint32_t)e->timestamp;
    snprintf(e->time, sizeof(e->time), "%02u:%02u:%02u", (unsigned)((s / 3600) % 24),
             (unsigned)((s / 60) % 60), (unsigned)(s % 60));
  }
  if (!e->date[0]) event_record_copy_str(e->date, sizeof(e->date), "1970-01-01");
}

static bool jsonAppend(char*& p, const char* end, const char* s) {
  if (!s) return true;
  while (*s) {
    if (p >= end - 1) return false;
    *p++ = *s++;
  }
  *p = 0;
  return true;
}

size_t event_record_json(const EventRecord& e, char* dst, size_t max) {
  if (!dst || max < 8) return 0;
  char* p = dst;
  const char* end = dst + max;
  *p = 0;
  char buf[480];
  snprintf(buf, sizeof(buf),
           "{\"id\":\"%s\",\"device_id\":\"%s\",\"point_id\":\"%s\",\"type\":\"%s\","
           "\"severity\":\"%s\",\"state\":\"%s\",\"timestamp\":%lld,\"sequence\":%lu,"
           "\"source\":\"%s\",\"mesh_ok\":%s,\"device_ok\":%s,"
           "\"date\":\"%s\",\"time\":\"%s\",\"zone\":\"%s\",\"user_id\":\"%s\","
           "\"name\":\"%s\",\"rfid_uid\":\"%s\",\"action\":\"%s\",\"status\":\"%s\","
           "\"remarks\":\"%s\"}",
           e.uuid, e.device_id, e.point_id, e.type, e.severity, e.state,
           (long long)e.timestamp, (unsigned long)e.sequence, e.source,
           e.mesh_ok ? "true" : "false", e.device_ok ? "true" : "false", e.date, e.time,
           e.zone_id, e.user_id, e.name, e.rfid_uid, e.action, e.status, e.remarks);
  if (!jsonAppend(p, end, buf)) return 0;
  return (size_t)(p - dst);
}

size_t event_record_ndjson(const EventRecord& e, const char* node, char* dst, size_t max) {
  if (!dst || max < 32) return 0;
  char* p = dst;
  const char* end = dst + max;
  *p = 0;
  const char* n = node && node[0] ? node : MESH485_NODE_ID;
  char head[192];
  snprintf(head, sizeof(head),
           "{\"v\":1,\"type\":\"m485.event\",\"node\":\"%s\",\"device\":\"%s\","
           "\"point\":\"%s\",\"event\":\"%s\",\"state\":\"%s\",\"severity\":\"%s\","
           "\"timestamp\":%lld,\"sequence\":%lu,\"source\":\"%s\",\"mesh_ok\":%s,"
           "\"device_ok\":%s,\"payload\":",
           n, e.device_id, e.point_id, e.type, e.state, e.severity, (long long)e.timestamp,
           (unsigned long)e.sequence, e.source, e.mesh_ok ? "true" : "false",
           e.device_ok ? "true" : "false");
  if (!jsonAppend(p, end, head)) return 0;
  char body[480];
  const size_t bn = event_record_json(e, body, sizeof(body));
  if (!bn) return 0;
  if (!jsonAppend(p, end, body)) return 0;
  if (!jsonAppend(p, end, "}")) return 0;
  return (size_t)(p - dst);
}
