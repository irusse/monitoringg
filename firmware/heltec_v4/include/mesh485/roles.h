#pragma once

#include <stdint.h>
#include <string.h>

// Envelope `operator` is a name. Authoritative role comes from the allow-list
// (`mesh485/auth.h`, RAM + `/m485_auth.bin`). This helper maps well-known keywords.
enum class Mesh485Role : uint8_t { Viewer = 0, Operator = 1, Admin = 2 };

inline bool mesh485_ieq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = *a++;
    char cb = *b++;
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

inline bool mesh485_parse_role(const char* s, Mesh485Role* out) {
  if (!s || !s[0] || !out) return false;
  if (mesh485_ieq(s, "admin")) {
    *out = Mesh485Role::Admin;
    return true;
  }
  if (mesh485_ieq(s, "operator")) {
    *out = Mesh485Role::Operator;
    return true;
  }
  if (mesh485_ieq(s, "viewer")) {
    *out = Mesh485Role::Viewer;
    return true;
  }
  return false;
}

inline Mesh485Role mesh485_role(const char* optr) {
  Mesh485Role r = Mesh485Role::Viewer;
  if (mesh485_parse_role(optr, &r)) return r;
  return Mesh485Role::Viewer;
}

inline bool mesh485_can_execute(Mesh485Role r) {
  return r == Mesh485Role::Operator || r == Mesh485Role::Admin;
}

// Never executable, even if a client asks. Not advertised on Device.capabilities.
inline bool mesh485_is_forbidden_cap(const char* cap) {
  if (!cap || !cap[0]) return false;
  static const char* const k[] = {
      "DISABLE_FIRE_ALARM",
      "DISARM_ALL",
      "ARM_ALL",
      "SILENCE",
      "SILENCE_ALARM",
      "FIRE_RESET",
      "RESET_ALARM",
      "DISABLE_ALARM",
      "FIRE_DISABLE",
      "DISARM",
      "ARM",
      nullptr};
  for (size_t i = 0; k[i]; ++i) {
    if (strcmp(cap, k[i]) == 0) return true;
  }
  return false;
}

// COMMAND_EXECUTE allow-list this run. READ_STATUS / READ_ZONES are not ops.
inline bool mesh485_is_executable_cap(const char* cap) {
  return cap && strcmp(cap, "CONTROL_RELAY") == 0;
}
