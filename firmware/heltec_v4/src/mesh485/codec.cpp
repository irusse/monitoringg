#include "mesh485/codec.h"

#include <stdio.h>
#include <string.h>

static const char* skipWs(const char* s) {
  while (s && *s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) ++s;
  return s;
}

static bool copyZ(char* dst, size_t max, const char* src, size_t n) {
  if (!dst || max == 0) return false;
  if (n >= max) n = max - 1;
  memcpy(dst, src, n);
  dst[n] = 0;
  return true;
}

static const char* parseString(const char* s, char* dst, size_t max) {
  s = skipWs(s);
  if (!s || *s != '"') return nullptr;
  ++s;
  size_t n = 0;
  while (*s && *s != '"') {
    char c = *s++;
    if (c == '\\') {
      if (!*s) return nullptr;
      const char e = *s++;
      if (e == '"' || e == '\\' || e == '/') c = e;
      else if (e == 'n') c = '\n';
      else if (e == 't') c = '\t';
      else c = e;
    }
    if (n + 1 < max) dst[n++] = c;
    else return nullptr;
  }
  if (*s != '"') return nullptr;
  dst[n] = 0;
  return s + 1;
}

static const char* parseInt64(const char* s, int64_t* out) {
  s = skipWs(s);
  if (!s || !*s) return nullptr;
  int sign = 1;
  if (*s == '-') {
    sign = -1;
    ++s;
  }
  if (*s < '0' || *s > '9') return nullptr;
  int64_t v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    ++s;
  }
  *out = v * sign;
  return s;
}

static const char* skipValue(const char* s) {
  s = skipWs(s);
  if (!s || !*s) return nullptr;
  if (*s == '"') {
    char tmp[8];
    return parseString(s, tmp, sizeof(tmp)) ? s : nullptr;
    // parseString may truncate — use a skip-string instead
  }
  if (*s == '{') {
    int depth = 1;
    bool in_str = false;
    bool esc = false;
    ++s;
    while (*s && depth) {
      const char c = *s++;
      if (in_str) {
        if (esc) esc = false;
        else if (c == '\\') esc = true;
        else if (c == '"') in_str = false;
      } else {
        if (c == '"') in_str = true;
        else if (c == '{') ++depth;
        else if (c == '}') --depth;
        else if (c == '[') ++depth;
        else if (c == ']') --depth;
      }
    }
    return depth == 0 ? s : nullptr;
  }
  if (*s == '[') {
    int depth = 1;
    bool in_str = false;
    bool esc = false;
    ++s;
    while (*s && depth) {
      const char c = *s++;
      if (in_str) {
        if (esc) esc = false;
        else if (c == '\\') esc = true;
        else if (c == '"') in_str = false;
      } else {
        if (c == '"') in_str = true;
        else if (c == '[' || c == '{') ++depth;
        else if (c == ']' || c == '}') --depth;
      }
    }
    return depth == 0 ? s : nullptr;
  }
  if (strncmp(s, "null", 4) == 0) return s + 4;
  if (strncmp(s, "true", 4) == 0) return s + 4;
  if (strncmp(s, "false", 5) == 0) return s + 5;
  if (*s == '-' || (*s >= '0' && *s <= '9')) {
    int64_t dummy = 0;
    return parseInt64(s, &dummy);
  }
  return nullptr;
}

static const char* skipStringKeep(const char* s) {
  s = skipWs(s);
  if (!s || *s != '"') return nullptr;
  ++s;
  bool esc = false;
  while (*s) {
    if (esc) {
      esc = false;
      ++s;
      continue;
    }
    if (*s == '\\') {
      esc = true;
      ++s;
      continue;
    }
    if (*s == '"') return s + 1;
    ++s;
  }
  return nullptr;
}

static const char* skipValueFixed(const char* s) {
  s = skipWs(s);
  if (!s || !*s) return nullptr;
  if (*s == '"') return skipStringKeep(s);
  return skipValue(s);
}

static bool parsePayload(const char* s, Mesh485Request* out) {
  s = skipWs(s);
  if (!s || *s != '{') return false;
  ++s;
  while (true) {
    s = skipWs(s);
    if (!s) return false;
    if (*s == '}') return true;
    if (*s == ',') {
      ++s;
      continue;
    }
    if (*s != '"') return false;
    char key[24] = {};
    s = parseString(s, key, sizeof(key));
    if (!s) return false;
    s = skipWs(s);
    if (!s || *s != ':') return false;
    ++s;
    s = skipWs(s);
    if (strcmp(key, "timeout_ms") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s || v < 0) return false;
      out->timeout_ms = (uint32_t)v;
    } else if (strcmp(key, "frame") == 0) {
      s = parseString(s, out->frame_b64, sizeof(out->frame_b64));
      if (!s) return false;
    } else if (strcmp(key, "protocol") == 0) {
      s = parseString(s, out->protocol, sizeof(out->protocol));
      if (!s) return false;
    } else if (strcmp(key, "level") == 0) {
      s = parseString(s, out->level, sizeof(out->level));
      if (!s) return false;
    } else if (strcmp(key, "idle_s") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s || v < 0) return false;
      out->idle_s = (uint32_t)v;
    } else if (strcmp(key, "device_id") == 0) {
      s = parseString(s, out->device_id, sizeof(out->device_id));
      if (!s) return false;
    } else if (strcmp(key, "point_id") == 0) {
      s = parseString(s, out->point_id, sizeof(out->point_id));
      if (!s) return false;
    } else if (strcmp(key, "since") == 0) {
      s = parseString(s, out->since, sizeof(out->since));
      if (!s) return false;
    } else if (strcmp(key, "stub") == 0) {
      s = parseString(s, out->stub, sizeof(out->stub));
      if (!s) return false;
    } else if (strcmp(key, "capability") == 0) {
      s = parseString(s, out->capability, sizeof(out->capability));
      if (!s) return false;
    } else if (strcmp(key, "op") == 0) {
      // Inner op = capability name (`COMMAND_EXECUTE` only if Device.capabilities
      // contains this string).
      if (!out->capability[0]) {
        s = parseString(s, out->capability, sizeof(out->capability));
        if (!s) return false;
      } else {
        s = skipValueFixed(s);
        if (!s) return false;
      }
    } else if (strcmp(key, "action") == 0 || strcmp(key, "command") == 0) {
      s = parseString(s, out->action, sizeof(out->action));
      if (!s) return false;
    } else if (strcmp(key, "uid") == 0) {
      s = parseString(s, out->uid, sizeof(out->uid));
      if (!s) return false;
    } else if (strcmp(key, "employee_id") == 0) {
      s = parseString(s, out->employee_id, sizeof(out->employee_id));
      if (!s) return false;
    } else if (strcmp(key, "name") == 0) {
      s = parseString(s, out->card_name, sizeof(out->card_name));
      if (!s) return false;
    } else if (strcmp(key, "pulse_ms") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s || v < 0) return false;
      out->pulse_ms = v > 5000 ? 5000 : (uint16_t)v;
    } else if (strcmp(key, "enabled") == 0) {
      s = skipWs(s);
      if (strncmp(s, "true", 4) == 0) {
        out->uid_enabled = 1;
        out->uid_enabled_set = 1;
        s += 4;
      } else if (strncmp(s, "false", 5) == 0) {
        out->uid_enabled = 0;
        out->uid_enabled_set = 1;
        s += 5;
      } else {
        return false;
      }
    } else if (strcmp(key, "offset") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s || v < 0) return false;
      out->list_offset = v > 65535 ? 65535 : (uint16_t)v;
    } else if (strcmp(key, "limit") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s || v < 0) return false;
      out->list_limit = v > 64 ? 64 : (uint16_t)v;
    } else if (strcmp(key, "role") == 0) {
      s = parseString(s, out->role, sizeof(out->role));
      if (!s) return false;
    } else if (strcmp(key, "pubkey") == 0) {
      s = skipWs(s);
      if (s && strncmp(s, "null", 4) == 0) {
        out->pubkey[0] = 0;
        out->pubkey_set = 1;
        s += 4;
      } else {
        s = parseString(s, out->pubkey, sizeof(out->pubkey));
        if (!s) return false;
        out->pubkey_set = 1;
      }
    } else {
      s = skipValueFixed(s);
      if (!s) return false;
    }
  }
}

bool mesh485_parse_request(const char* json, Mesh485Request* out) {
  if (!json || !out) return false;
  memset(out, 0, sizeof(*out));
  out->timeout_ms = MESH485_TXN_TIMEOUT_DEFAULT_MS;
  out->pulse_ms = 300;
  out->uid_enabled = 1;
  const char* s = skipWs(json);
  if (!s || *s != '{') return false;
  ++s;
  while (true) {
    s = skipWs(s);
    if (!s) return false;
    if (*s == '}') break;
    if (*s == ',') {
      ++s;
      continue;
    }
    if (*s != '"') return false;
    char key[16] = {};
    s = parseString(s, key, sizeof(key));
    if (!s) return false;
    s = skipWs(s);
    if (!s || *s != ':') return false;
    ++s;
    s = skipWs(s);
    if (strcmp(key, "v") == 0) {
      int64_t v = 0;
      s = parseInt64(s, &v);
      if (!s) return false;
      out->v = (uint8_t)v;
    } else if (strcmp(key, "type") == 0) {
      char t[24] = {};
      s = parseString(s, t, sizeof(t));
      if (!s) return false;
      if (strcmp(t, "m485.request") == 0) out->type = Mesh485Type::Request;
      else if (strcmp(t, "m485.response") == 0) out->type = Mesh485Type::Response;
      else if (strcmp(t, "m485.event") == 0) out->type = Mesh485Type::Event;
    } else if (strcmp(key, "id") == 0) {
      s = parseString(s, out->id, sizeof(out->id));
      if (!s) return false;
    } else if (strcmp(key, "node") == 0) {
      s = parseString(s, out->node, sizeof(out->node));
      if (!s) return false;
    } else if (strcmp(key, "op") == 0) {
      s = parseString(s, out->op, sizeof(out->op));
      if (!s) return false;
    } else if (strcmp(key, "operator") == 0) {
      s = parseString(s, out->optr, sizeof(out->optr));
      if (!s) return false;
    } else if (strcmp(key, "ts") == 0) {
      s = parseInt64(s, &out->ts);
      if (!s) return false;
    } else if (strcmp(key, "nonce") == 0) {
      s = parseString(s, out->nonce, sizeof(out->nonce));
      if (!s) return false;
    } else if (strcmp(key, "payload") == 0) {
      if (*s == '{') {
        if (!parsePayload(s, out)) return false;
        s = skipValueFixed(s);
        if (!s) return false;
      } else {
        s = skipValueFixed(s);
        if (!s) return false;
      }
    } else {
      s = skipValueFixed(s);
      if (!s) return false;
    }
  }
  return out->v == 1 && out->type == Mesh485Type::Request && out->op[0] != 0 &&
         out->id[0] != 0;
}

static bool append(char*& p, const char* end, const char* s) {
  if (!s) return true;
  while (*s) {
    if (p >= end - 1) return false;
    *p++ = *s++;
  }
  *p = 0;
  return true;
}

size_t mesh485_write_response(char* buf, size_t max, const Mesh485Response& resp) {
  if (!buf || max < 32) return 0;
  char* p = buf;
  const char* end = buf + max;
  if (!append(p, end, "{\"v\":1,\"type\":\"m485.response\",\"id\":\"")) return 0;
  if (!append(p, end, resp.id[0] ? resp.id : "0")) return 0;
  if (!append(p, end, "\",\"node\":\"")) return 0;
  if (!append(p, end, resp.node[0] ? resp.node : MESH485_NODE_ID)) return 0;
  if (!append(p, end, "\",\"op\":\"")) return 0;
  if (!append(p, end, resp.op[0] ? resp.op : "UNKNOWN")) return 0;
  if (!append(p, end, "\",\"result\":\"")) return 0;
  if (!append(p, end, resp.result[0] ? resp.result : "ERR")) return 0;
  if (!append(p, end, "\",\"error\":")) return 0;
  if (resp.error[0]) {
    if (!append(p, end, "\"")) return 0;
    if (!append(p, end, resp.error)) return 0;
    if (!append(p, end, "\"")) return 0;
  } else {
    if (!append(p, end, "null")) return 0;
  }
  if (!append(p, end, ",\"ts\":")) return 0;
  char nbuf[24];
  snprintf(nbuf, sizeof(nbuf), "%lld", (long long)resp.ts);
  if (!append(p, end, nbuf)) return 0;
  if (!append(p, end, ",\"payload\":")) return 0;
  if (!append(p, end, resp.payload[0] ? resp.payload : "{}")) return 0;
  if (!append(p, end, "}")) return 0;
  return (size_t)(p - buf);
}
