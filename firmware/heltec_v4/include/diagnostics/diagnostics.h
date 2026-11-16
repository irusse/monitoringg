#pragma once

#include <stdint.h>

struct DiagnosticsSnapshot {
  bool mesh_ok;
  bool repeater;
  bool wifi;
  uint32_t heap;
  uint32_t uptime_s;
};

DiagnosticsSnapshot capture_diagnostics();
