/// Node ACL row from ACCESS_GET_USERS. No photo bytes on the wire.
class AccessUser {
  const AccessUser({
    required this.uid,
    required this.enabled,
    this.employeeId,
    this.name,
  });

  final String uid;
  final bool enabled;
  final String? employeeId;
  final String? name;

  String get displayName =>
      (name != null && name!.isNotEmpty) ? name! : uid;

  String get photoKey =>
      (employeeId != null && employeeId!.isNotEmpty) ? employeeId! : uid;

  factory AccessUser.fromJson(Map<String, dynamic> json) {
    return AccessUser(
      uid: (json['uid'] as String? ?? '').toUpperCase(),
      enabled: json['enabled'] as bool? ?? true,
      employeeId: json['employee_id'] as String?,
      name: json['name'] as String?,
    );
  }
}

class AccessUsersPage {
  const AccessUsersPage({
    required this.users,
    this.total = 0,
    this.offset = 0,
    this.error,
  });

  final List<AccessUser> users;
  final int total;
  final int offset;
  final String? error;

  bool get denied => error == 'NO_PERMISSION' || error == 'BAD_AUTH';
}

class AccessStatus {
  const AccessStatus({
    required this.device,
    required this.state,
    required this.users,
    this.lastUid,
  });

  final String device;
  final String state;
  final int users;
  final String? lastUid;

  factory AccessStatus.fromJson(Map<String, dynamic> json) {
    return AccessStatus(
      device: json['device'] as String? ?? 'TS-001',
      state: json['state'] as String? ?? 'UNKNOWN',
      users: (json['users'] as num?)?.toInt() ?? 0,
      lastUid: json['last_uid'] as String?,
    );
  }
}

/// Canonical Wiegand UID: uppercase hex, no spaces/colons.
String canonicalizeUid(String raw) {
  final out = StringBuffer();
  for (final unit in raw.toUpperCase().codeUnits) {
    final c = String.fromCharCode(unit);
    if ((c.compareTo('0') >= 0 && c.compareTo('9') <= 0) ||
        (c.compareTo('A') >= 0 && c.compareTo('F') <= 0)) {
      out.write(c);
    }
  }
  return out.toString();
}
