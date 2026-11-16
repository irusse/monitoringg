import '../models/access_user.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/mesh_event.dart';
import '../models/node_health.dart';
import '../models/point.dart';
import 'mesh485_client.dart';

/// In-memory MESH 485 JSON. Device ids match firmware registry:
/// RAW-001 / TS-001 / PP-001 / S10-001 / S20-001 / PCN-001.
/// PP/S10/S20 empty bus → online:false. PCN-001 is the GPIO contact (read only).
///
/// Golden request/response envelopes live in `protocol/mesh485/examples/`.
/// ADMIN add-card succeeds against RAM ACL (cap 64). OPEN/LOCK stay honest TIMEOUT.
class FixtureMesh485Client implements Mesh485Client {
  FixtureMesh485Client();

  final List<AccessUser> _acl = [];
  static const int _aclCap = 64;
  static const _factoryNames = {'viewer', 'operator', 'admin'};

  CommandAck? _refuseUnknown(String operatorName) {
    if (!_factoryNames.contains(operatorName.toLowerCase())) {
      return CommandAck.badAuthAck;
    }
    return null;
  }

  @override
  String get transportLabel => 'fixture (in-memory JSON)';

  static const _devices = <Map<String, dynamic>>[
    {
      'id': 'RAW-001',
      'vendor': 'generic',
      'model': 'RS485',
      'protocol': 'RAW_RS485',
      'address': 0,
      'online': true,
      'capabilities': ['READ_STATUS'],
      'state': 'OK',
    },
    {
      'id': 'TS-001',
      'vendor': 'access',
      'model': 'TURNSTILE',
      'protocol': 'ACCESS',
      'address': 0,
      'online': true,
      'capabilities': ['READ_STATUS', 'ACCESS_OPEN'],
      'state': 'OK',
    },
    {
      'id': 'PP-001',
      'vendor': 'bolid',
      'model': 'C2000PP',
      'protocol': 'BOLID_C2000PP',
      'address': 1,
      'online': false,
      'capabilities': ['READ_STATUS', 'CONTROL_RELAY'],
      'state': 'OFFLINE',
    },
    {
      'id': 'S10-001',
      'vendor': 'bolid',
      'model': 'SIGNAL10',
      'protocol': 'BOLID_SIGNAL10',
      'address': 1,
      'online': false,
      'capabilities': ['READ_STATUS'],
      'state': 'OFFLINE',
    },
    {
      'id': 'S20-001',
      'vendor': 'bolid',
      'model': 'SIGNAL20',
      'protocol': 'BOLID_SIGNAL20',
      'address': 1,
      'online': false,
      'capabilities': ['READ_STATUS'],
      'state': 'OFFLINE',
    },
    {
      'id': 'PCN-001',
      'vendor': 'local',
      'model': 'PCN',
      'protocol': 'PCN_CONTACT',
      'address': 0,
      'online': true,
      'capabilities': ['READ_STATUS'],
      'state': 'ALARM',
    },
  ];

  static const _points = <Map<String, dynamic>>[
    {
      'device_id': 'RAW-001',
      'point_id': 'BUS',
      'type': 'loop',
      'name': 'RS-485',
      'state': 'UNKNOWN',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'TS-001',
      'point_id': 'TURNSTILE',
      'type': 'turnstile',
      'name': 'Turnstile',
      'state': 'LOCKED',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'TS-001',
      'point_id': 'READER',
      'type': 'card_reader',
      'name': 'Reader',
      'state': 'UNKNOWN',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'PP-001',
      'point_id': 'PP',
      'type': 'loop',
      'name': 'C2000-PP',
      'state': 'FAULT',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'S10-001',
      'point_id': 'S10',
      'type': 'loop',
      'name': 'Signal-10',
      'state': 'FAULT',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'S20-001',
      'point_id': 'S20',
      'type': 'loop',
      'name': 'Signal-20',
      'state': 'FAULT',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'PCN-001',
      'point_id': 'ALARM',
      'type': 'PCN',
      'name': 'PCN',
      'state': 'ALARM',
      'attributes': <String, dynamic>{},
    },
    {
      'device_id': 'PP-001',
      'point_id': 'R1',
      'type': 'relay',
      'name': 'Relay 1',
      'state': 'UNKNOWN',
      'attributes': <String, dynamic>{},
    },
  ];

  /// Fixture rows (golden EVENT_LIST is `[]` on an empty bus). Fake data is OK;
  /// PP-001 must not invent FIRE / type=36.
  static const _events = <Map<String, dynamic>>[
    {
      'id': '00000000-0000-4000-8000-000000000001',
      'device_id': 'TS-001',
      'point_id': 'READER',
      'type': 'CARD',
      'severity': 'INFO',
      'state': 'SEEN',
      'timestamp': 1787912345,
      'sequence': 1,
      'source': 'RFID',
      'mesh_ok': true,
      'device_ok': true,
      'rfid_uid': 'STUBCARD',
      'action': 'ENTRY',
      'status': 'Authorized',
    },
    {
      'id': '00000000-0000-4000-8000-000000000002',
      'device_id': 'RAW-001',
      'point_id': 'BUS',
      'type': 'DIAG',
      'severity': 'INFO',
      'state': 'IDLE_RX',
      'timestamp': 1787912300,
      'sequence': 2,
      'source': 'RS485',
      'mesh_ok': true,
      'device_ok': false,
    },
    {
      'id': '00000000-0000-4000-8005-000000000001',
      'device_id': 'PCN-001',
      'point_id': 'ALARM',
      'type': 'PCN',
      'severity': 'ALARM',
      'state': 'ALARM',
      'timestamp': 1787912400,
      'sequence': 3,
      'source': 'GPIO',
      'mesh_ok': false,
      'device_ok': true,
    },
  ];

  @override
  Future<List<DeviceRecord>> discoverDevices() async {
    return _devices.map(DeviceRecord.fromJson).toList();
  }

  @override
  Future<DeviceRecord> deviceInfo(String deviceId) async {
    final row = _devices.firstWhere(
      (d) => d['id'] == deviceId,
      orElse: () => throw ArgumentError('unknown device_id: $deviceId'),
    );
    return DeviceRecord.fromJson(row);
  }

  @override
  Future<List<PointRecord>> pointList({String? deviceId}) async {
    final rows = deviceId == null
        ? _points
        : _points.where((p) => p['device_id'] == deviceId);
    return rows.map(PointRecord.fromJson).toList();
  }

  @override
  Future<List<MeshEvent>> eventList() async {
    return _events.map(MeshEvent.fromJson).toList();
  }

  @override
  Future<NodeHealth> nodeStatus() async {
    return const NodeHealth(
      node: 'EXT-001',
      radio: 'ok',
      repeater: true,
      uptimeS: 12,
      profile: 'access+rs485',
      devices: 6,
      rssi: -80,
      snr: 7.5,
      rx: 1,
      tx: 2,
      lastSeen: 'fixture',
      meshOk: true,
      deviceOk: false,
      rs485Ok: true,
      rs485Baud: 9600,
      rs485Protocol: 'RAW',
      session: 'off',
      queued: 2,
      pendingMesh: 0,
    );
  }

  @override
  Future<BridgeStatus> bridgeStatus() async {
    return const BridgeStatus(
      session: 'off',
      level: 'READ_ONLY',
      port: 'RS485',
    );
  }

  @override
  Future<List<SiteObject>> listObjects() async {
    return const [
      SiteObject(
        id: 'SITE-001',
        name: 'Стенд',
        deviceIds: ['RAW-001', 'TS-001', 'PP-001', 'S10-001', 'S20-001', 'PCN-001'],
        note:
            'C2000-ПП = PP-001. Signal-10 = S10-001. Signal-20 = S20-001. Турникет = TS-001. RAW шина. ПЦН = PCN-001.',
      ),
    ];
  }

  @override
  Future<CommandAck> accessOpen({String operatorName = 'viewer'}) async {
    final unknown = _refuseUnknown(operatorName);
    if (unknown != null) return unknown;
    final role = operatorName.toLowerCase();
    if (role != 'operator' && role != 'admin') {
      return CommandAck.deniedAck;
    }
    return const CommandAck(
      meshOk: true,
      deviceOk: false,
      error: 'TIMEOUT',
      result: 'ERR',
      deviceId: 'TS-001',
      capability: 'ACCESS_OPEN',
      pointId: 'TURNSTILE',
    );
  }

  @override
  Future<CommandAck> accessLock({String operatorName = 'viewer'}) async {
    final unknown = _refuseUnknown(operatorName);
    if (unknown != null) return unknown;
    final role = operatorName.toLowerCase();
    if (role != 'operator' && role != 'admin') {
      return CommandAck.deniedAck;
    }
    return const CommandAck(
      meshOk: true,
      deviceOk: false,
      error: 'TIMEOUT',
      result: 'ERR',
      deviceId: 'TS-001',
      capability: 'ACCESS_LOCK',
      pointId: 'TURNSTILE',
    );
  }

  @override
  Future<AccessStatus> accessStatus({String operatorName = 'viewer'}) async {
    final enabled = _acl.where((u) => u.enabled).length;
    return AccessStatus(
      device: 'TS-001',
      state: 'LOCKED',
      users: enabled,
    );
  }

  @override
  Future<AccessUsersPage> accessGetUsers({
    String operatorName = 'viewer',
    int offset = 0,
    int limit = 16,
  }) async {
    if (!_factoryNames.contains(operatorName.toLowerCase())) {
      return const AccessUsersPage(users: [], error: 'BAD_AUTH');
    }
    if (operatorName.toLowerCase() != 'admin') {
      return const AccessUsersPage(
        users: [],
        error: 'NO_PERMISSION',
      );
    }
    final cap = limit < 1 ? 16 : (limit > 16 ? 16 : limit);
    final start = offset < 0 ? 0 : offset;
    final slice = start >= _acl.length
        ? const <AccessUser>[]
        : _acl.sublist(start, start + cap > _acl.length ? _acl.length : start + cap);
    return AccessUsersPage(
      users: slice,
      total: _acl.length,
      offset: start,
    );
  }

  @override
  Future<CommandAck> accessAddCard({
    required String uid,
    String? employeeId,
    String? name,
    bool enabled = true,
    String operatorName = 'viewer',
  }) async {
    final unknown = _refuseUnknown(operatorName);
    if (unknown != null) return unknown;
    if (operatorName.toLowerCase() != 'admin') {
      return CommandAck.deniedAck;
    }
    final canon = canonicalizeUid(uid);
    if (canon.isEmpty) {
      return const CommandAck(
        meshOk: true,
        deviceOk: false,
        error: 'UNKNOWN_CMD',
        result: 'ERR',
      );
    }
    final idx = _acl.indexWhere((u) => u.uid == canon);
    if (idx >= 0) {
      _acl[idx] = AccessUser(
        uid: canon,
        enabled: enabled,
        employeeId: employeeId,
        name: name,
      );
    } else {
      if (_acl.length >= _aclCap) {
        return const CommandAck(
          meshOk: true,
          deviceOk: false,
          error: 'BUSY',
          result: 'ERR',
        );
      }
      _acl.add(AccessUser(
        uid: canon,
        enabled: enabled,
        employeeId: employeeId,
        name: name,
      ));
    }
    return CommandAck(
      meshOk: true,
      deviceOk: true,
      result: 'OK',
      deviceId: 'TS-001',
      capability: 'ACCESS_ADD_CARD',
    );
  }

  @override
  Future<CommandAck> accessRemoveCard({
    required String uid,
    String operatorName = 'viewer',
  }) async {
    final unknown = _refuseUnknown(operatorName);
    if (unknown != null) return unknown;
    if (operatorName.toLowerCase() != 'admin') {
      return CommandAck.deniedAck;
    }
    final canon = canonicalizeUid(uid);
    final idx = _acl.indexWhere((u) => u.uid == canon);
    if (idx < 0) {
      return const CommandAck(
        meshOk: true,
        deviceOk: false,
        error: 'UNKNOWN_CMD',
        result: 'ERR',
      );
    }
    _acl.removeAt(idx);
    return const CommandAck(
      meshOk: true,
      deviceOk: true,
      result: 'OK',
      deviceId: 'TS-001',
      capability: 'ACCESS_REMOVE_CARD',
    );
  }

  @override
  Future<List<MeshEvent>> accessGetEvents({String operatorName = 'viewer'}) async {
    return _events
        .where((e) => e['device_id'] == 'TS-001')
        .map((e) => MeshEvent.fromJson(Map<String, dynamic>.from(e)))
        .toList();
  }

  static const _forbidden = {
    'DISABLE_FIRE_ALARM',
    'DISARM_ALL',
    'ARM_ALL',
    'SILENCE',
    'SILENCE_ALARM',
    'FIRE_RESET',
    'RESET_ALARM',
    'DISABLE_ALARM',
    'FIRE_DISABLE',
    'DISARM',
    'ARM',
  };

  /// Fixture seam: PP-001 advertises CONTROL_RELAY + R1 so the Control UI can
  /// be exercised without a live converter. Firmware default allow-list is empty
  /// until panel config enables a named relay. Empty-bus result is TIMEOUT.
  @override
  Future<CommandAck> commandExecute({
    required String deviceId,
    required String capability,
    required String pointId,
    required String action,
    String operatorName = 'viewer',
  }) async {
    final unknown = _refuseUnknown(operatorName);
    if (unknown != null) return unknown;
    final role = operatorName.toLowerCase();
    if (role != 'operator' && role != 'admin') {
      return CommandAck.deniedAck;
    }
    if (_forbidden.contains(capability) || capability != 'CONTROL_RELAY') {
      return CommandAck.deniedAck;
    }
    DeviceRecord device;
    try {
      device = await deviceInfo(deviceId);
    } catch (_) {
      return CommandAck.deniedAck;
    }
    if (!device.capabilities.contains(capability)) {
      return CommandAck.deniedAck;
    }
    final pts = await pointList(deviceId: deviceId);
    final mapped = pts.any((p) => p.pointId == pointId && p.type == 'relay');
    if (!mapped) return CommandAck.deniedAck;
    return CommandAck(
      meshOk: true,
      deviceOk: false,
      error: 'TIMEOUT',
      result: 'ERR',
      deviceId: deviceId,
      capability: capability,
      pointId: pointId,
    );
  }
}
