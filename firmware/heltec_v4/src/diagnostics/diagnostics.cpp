#include "diagnostics/diagnostics.h"
#include <Arduino.h>

DiagnosticsSnapshot capture_diagnostics() {
  DiagnosticsSnapshot s{};
  s.uptime_s = millis() / 1000;
  return s;
}
