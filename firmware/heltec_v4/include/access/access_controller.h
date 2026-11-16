#pragma once

#include <stddef.h>
#include <stdint.h>

enum class AccessResult { Allow, Deny };

struct AccessDecision {
  AccessResult result;
  char employee_id[32];
  char name[32];
  char reason[32];
};

#ifndef ACCESS_ACL_CAP
#define ACCESS_ACL_CAP 64
#endif

#ifndef MESH485_TEST_UID
#define MESH485_TEST_UID "04A79122"
#endif

// Packed so SPIFFS / native fake-FS blobs stay stable across reboot.
#pragma pack(push, 1)
struct AccessAclRecord {
  char uid[24];
  uint8_t enabled;
  uint8_t pad[3];
  char employee_id[32];
  char name[32];
};

struct AccessAclHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint16_t cap;
  uint16_t reserved;
};
#pragma pack(pop)

// Native: RAM only unless access_acl_use_fake_fs(true) for reboot tests.
void access_acl_use_fake_fs(bool on);
void access_acl_reset_fake_fs();

// Offline ACL. Empty store → unknown UID = DENY. SPIFFS `/m485_acl.bin`.
class AccessController {
public:
  static constexpr size_t kCap = ACCESS_ACL_CAP;
  static constexpr uint32_t kMagic = 0x41434C31u;  // 'ACL1'

  void begin();
  void ensureBegun() {
    if (!ready_) begin();
  }
  AccessDecision checkUid(const char* uid);
  bool allowUid(const char* hex);
  bool addCard(const char* uid, const char* employee_id, const char* name, bool enabled);
  bool removeCard(const char* uid);
  uint16_t count() const { return count_; }
  uint16_t enabledCount() const;
  const AccessAclRecord* recordAt(uint16_t i) const;
  const char* lastUid() const { return last_uid_; }
  static bool canonicalizeUid(const char* in, char* out, size_t max);

private:
  AccessAclRecord* recs_ = nullptr;
  uint16_t count_ = 0;
  bool ready_ = false;
  char last_uid_[24] = {};

  bool ensureBuf();
  int findUid(const char* canon) const;
  bool persist();
  bool load();
  void resetRam();
  void seedTestUid();
};
