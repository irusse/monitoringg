#pragma once

#include <stddef.h>
#include <stdint.h>

// Packed so SPIFFS / native fake-FS blobs stay stable across reboot.
// One field: bus protocol. Not baud, not parity, not AUTO, not encrypted.
#pragma pack(push, 1)
struct Mesh485CfgBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  char protocol[16];
};
#pragma pack(pop)

// Native: RAM only unless mesh485_cfg_use_fake_fs(true) for reboot tests.
void mesh485_cfg_use_fake_fs(bool on);
void mesh485_cfg_reset_fake_fs();
void mesh485_cfg_poke_fake_fs(const uint8_t* data, size_t len);
size_t mesh485_cfg_fake_fs_len();
size_t mesh485_cfg_fake_fs_copy(uint8_t* dst, size_t max);

// Persisted CONFIG protocol (`/m485_cfg.bin`). Default RAW. Not a secret.
class Mesh485BusCfg {
public:
  static constexpr uint32_t kMagic = 0x43464731u;  // 'CFG1'
  static constexpr uint16_t kVersion = 1;

  Mesh485BusCfg();

  // Load `/m485_cfg.bin` (or native fake-fs). Missing / bad magic /
  // unknown protocol / AUTO in file → RAW (logged, not silent).
  void begin();

  const char* protocol() const { return protocol_; }
  // RAM + write after a successful CONFIG_SET. Refuses AUTO / unknown.
  bool setAndPersist(const char* proto);

private:
  char protocol_[16];

  void resetRam();
  bool persist() const;
  bool load(const char** why);
};
