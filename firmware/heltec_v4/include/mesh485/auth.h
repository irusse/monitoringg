#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mesh485/roles.h"

#ifndef MESH485_AUTH_CAP
#define MESH485_AUTH_CAP 8
#endif

#ifndef MESH485_PUBKEY_LEN
#define MESH485_PUBKEY_LEN 32
#endif

// Packed so SPIFFS / native fake-FS blobs stay stable across reboot.
// v1 (phase 21): name + role, no pubkey. v2 adds 32-byte pubkey (zeros = unbound).
#pragma pack(push, 1)
struct Mesh485AuthRecord {
  char name[24];
  uint8_t role;
  uint8_t pad[3];
};

struct Mesh485AuthRecordV2 {
  char name[24];
  uint8_t role;
  uint8_t pad[3];
  uint8_t pubkey[MESH485_PUBKEY_LEN];
};

struct Mesh485AuthHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint16_t cap;
  uint16_t reserved;
};
#pragma pack(pop)

struct Mesh485AuthEntry {
  char name[24];
  Mesh485Role role;
  uint8_t pubkey[MESH485_PUBKEY_LEN];
};

bool mesh485_parse_pubkey_hex(const char* hex, uint8_t out[MESH485_PUBKEY_LEN]);
bool mesh485_pubkey_empty(const uint8_t p[MESH485_PUBKEY_LEN]);

// Native: RAM only unless mesh485_auth_use_fake_fs(true) for reboot tests.
void mesh485_auth_use_fake_fs(bool on);
void mesh485_auth_reset_fake_fs();
void mesh485_auth_poke_fake_fs(const uint8_t* data, size_t len);

// Operator allow-list. Factory seed matches the Flutter login chip:
// viewer / operator / admin. Persisted as `/m485_auth.bin` (not encrypted).
class Mesh485Auth {
public:
  static constexpr uint32_t kMagic = 0x41555431u;  // 'AUT1'
  static constexpr uint16_t kVersionV1 = 1;
  static constexpr uint16_t kVersion = 2;

  Mesh485Auth();

  // Load `/m485_auth.bin` (or native fake-fs). Missing / bad magic / cap
  // overflow / no ADMIN → factory viewer/operator/admin (logged, not silent).
  // Version 1 blobs load as unbound rows (no factory reset).
  void begin();

  // Name-only lookup (no claimed-role / pubkey check).
  bool lookup(const char* name, Mesh485Role* out) const;
  bool hasPubkey(const char* name) const;
  bool pubkeyOf(const char* name, uint8_t out[MESH485_PUBKEY_LEN]) const;
  // Unknown name, claimed-up role, or bound row without matching presented
  // key → false (BAD_AUTH). On success *out is the stored role.
  bool resolve(const char* name, Mesh485Role* out) const {
    return resolve(name, nullptr, false, out);
  }
  bool resolve(const char* name, const uint8_t* presented, bool have_presented,
               Mesh485Role* out) const;
  Mesh485Role roleOf(const char* name) const;

  bool set(const char* name, Mesh485Role role);
  bool setPubkey(const char* name, const uint8_t pub[MESH485_PUBKEY_LEN]);
  bool clearPubkey(const char* name);
  bool remove(const char* name);
  // Write v2 blob after AUTH_SET. Refuses a table with no ADMIN.
  bool persist() const;
  uint8_t count() const { return n_; }
  uint8_t adminCount() const;
  const Mesh485AuthEntry* at(uint8_t i) const;

private:
  Mesh485AuthEntry rows_[MESH485_AUTH_CAP] = {};
  uint8_t n_ = 0;

  int find(const char* name) const;
  void resetRam();
  void seedFactory();
  bool load();
  bool rowsHaveAdmin() const;
};
