import 'dart:async';
import 'dart:convert';
import 'dart:math';

import '../models/access_user.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/mesh_event.dart';
import '../models/node_health.dart';
import '../models/point.dart';
import 'mesh485_client.dart';

/// Byte pipe that carries MESH 485 NDJSON (`{` … `}\n`). Not Companion frames.
abstract class Mesh485Link {
  String get label;
  Future<void> open();
  Future<void> close();
  Future<void> writeLine(String line);
  Stream<String> get lines;
}

/// Live MESH 485 client. Ignores Companion / boot noise.
class LiveMesh485Client implements Mesh485Client {
  LiveMesh485Client(this.link, {this.subscribeEvents = true, this.operatorName = 'viewer'});

  final Mesh485Link link;
  final bool subscribeEvents;
  String operatorName;

  final _events = <MeshEvent>[];
  final _pending = <String, Completer<Map<String, dynamic>>>{};
  StreamSubscription<String>? _sub;
  int _seq = 0;
  bool _opened = false;

  @override
  String get transportLabel => link.label;

  Future<void> ensureOpen() async {
    if (_opened) return;
    await link.open();
    _sub = link.lines.listen(_onLine, onError: (_) {});
    _opened = true;
    if (subscribeEvents) {
      try {
        await rpc('EVENT_SUBSCRIBE', {});
      } catch (_) {}
    }
  }

  Future<void> shutdown() async {
    await _sub?.cancel();
    _sub = null;
    await link.close();
    _opened = false;
  }

  void _onLine(String text) {
    final t = text.trim();
    if (!t.startsWith('{')) return;
    Map<String, dynamic> obj;
    try {
      obj = jsonDecode(t) as Map<String, dynamic>;
    } catch (_) {
      return;
    }
    final type = obj['type'] as String?;
    if (type == 'm485.event') {
      final payload = obj['payload'];
      if (payload is Map<String, dynamic>) {
        try {
          _events.insert(0, MeshEvent.fromJson(payload));
        } catch (_) {}
      }
      return;
    }
    if (type != 'm485.response') return;
    final id = obj['id'] as String?;
    if (id != null && _pending.containsKey(id)) {
      _pending.remove(id)!.complete(obj);
    }
  }

  String _nextId() {
    _seq = (_seq + 1) & 0xFFFFFF;
    return _seq.toRadixString(16).toUpperCase().padLeft(6, '0');
  }

  Future<Map<String, dynamic>> rpc(
    String op, [
    Map<String, dynamic> payload = const {},
  ]) async {
    await ensureOpen();
    final id = _nextId();
    final req = <String, dynamic>{
      'v': 1,
      'type': 'm485.request',
      'id': id,
      'node': 'EXT-001',
      'op': op,
      'operator': operatorName,
      'ts': DateTime.now().millisecondsSinceEpoch ~/ 1000,
      'nonce': Random().nextInt(0xFFFF).toRadixString(16),
      'payload': payload,
    };
    final line = jsonEncode(req);
    final c = Completer<Map<String, dynamic>>();
    _pending[id] = c;
    await link.writeLine(line);
    try {
      return await c.future.timeout(const Duration(seconds: 8));
    } catch (e) {
      _pending.remove(id);
      rethrow;
    }
  }

  Map<String, dynamic> _payload(Map<String, dynamic> resp) {
    final p = resp['payload'];
    if (p is Map<String, dynamic>) return p;
    return const {};
  }

  @override
  Future<List<DeviceRecord>> discoverDevices() async {
    final resp = await rpc('DEVICE_DISCOVER');
    final payload = _payload(resp);
    final rows = payload['devices'] as List<dynamic>? ?? const [];
    final out = <DeviceRecord>[];
    for (final row in rows) {
      if (row is! Map) continue;
      final id = row['id'] as String?;
      if (id == null) continue;
      try {
        final info = _payload(await rpc('DEVICE_INFO', {'device_id': id}));
        final st = _payload(await rpc('DEVICE_STATUS', {'device_id': id}));
        final merged = Map<String, dynamic>.from(info);
        if (st['state'] != null) merged['state'] = st['state'];
        if (st['online'] is bool) merged['online'] = st['online'];
        out.add(DeviceRecord.fromJson(merged));
      } catch (_) {
        out.add(DeviceRecord.fromJson(Map<String, dynamic>.from(row)));
      }
    }
    return out;
  }

  @override
  Future<DeviceRecord> deviceInfo(String deviceId) async {
    final info = _payload(await rpc('DEVICE_INFO', {'device_id': deviceId}));
    return DeviceRecord.fromJson(info);
  }

  @override
  Future<List<PointRecord>> pointList({String? deviceId}) async {
    final payload = deviceId == null
        ? <String, dynamic>{}
        : <String, dynamic>{'device_id': deviceId};
    final resp = _payload(await rpc('POINT_LIST', payload));
    final rows = resp['points'] as List<dynamic>? ?? const [];
    return rows
        .whereType<Map>()
        .map((e) => PointRecord.fromJson(Map<String, dynamic>.from(e)))
        .toList();
  }

  @override
  Future<List<MeshEvent>> eventList() async {
    final resp = _payload(await rpc('EVENT_LIST', {'since': '0'}));
    final rows = resp['events'] as List<dynamic>? ?? const [];
    final listed = rows
        .whereType<Map>()
        .map((e) => MeshEvent.fromJson(Map<String, dynamic>.from(e)))
        .toList();
    if (listed.isNotEmpty) return listed;
    return List<MeshEvent>.from(_events);
  }

  @override
  Future<NodeHealth> nodeStatus() async {
    final ns = _payload(await rpc('NODE_STATUS'));
    Map<String, dynamic> diag = const {};
    try {
      diag = _payload(await rpc('DIAGNOSTICS'));
    } catch (_) {}
    final rs485 = ns['rs485'];
    final rs485Map = rs485 is Map ? Map<String, dynamic>.from(rs485) : <String, dynamic>{};
    final session = ns['session'];
    String sessionLabel = 'off';
    if (session is String) {
      sessionLabel = session;
    } else if (session is Map) {
      sessionLabel = '${session['level']} idle=${session['idle_s']}s';
    }
    final events = diag['events'];
    final evMap = events is Map ? Map<String, dynamic>.from(events) : <String, dynamic>{};
    return NodeHealth(
      node: 'EXT-001',
      radio: ns['radio'] as String? ?? 'error',
      repeater: ns['repeater'] as bool? ?? false,
      uptimeS: (ns['uptime_s'] as num?)?.toInt() ?? 0,
      profile: ns['profile'] as String? ?? 'access+rs485',
      devices: (ns['devices'] as num?)?.toInt() ?? 0,
      rssi: (diag['rssi'] as num?)?.toInt() ?? 0,
      snr: (diag['snr'] as num?)?.toDouble() ?? 0,
      rx: (diag['rx'] as num?)?.toInt() ?? 0,
      tx: (diag['tx'] as num?)?.toInt() ?? 0,
      lastSeen: 'live',
      meshOk: diag['mesh_ok'] as bool? ?? (ns['radio'] == 'ok'),
      deviceOk: diag['device_ok'] as bool? ?? false,
      rs485Ok: rs485Map['ok'] as bool? ?? false,
      rs485Baud: (rs485Map['baud'] as num?)?.toInt() ?? 9600,
      rs485Protocol: rs485Map['protocol'] as String? ?? 'RAW',
      session: sessionLabel,
      queued: (evMap['queued'] as num?)?.toInt() ?? 0,
      pendingMesh: (evMap['pending_mesh'] as num?)?.toInt() ?? 0,
    );
  }

  @override
  Future<BridgeStatus> bridgeStatus() async {
    final ns = _payload(await rpc('NODE_STATUS'));
    final session = ns['session'];
    if (session is Map) {
      return BridgeStatus(
        session: '${session['level']}',
        level: '${session['level'] ?? 'READ_ONLY'}',
        port: 'RS485',
      );
    }
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
  Future<CommandAck> commandExecute({
    required String deviceId,
    required String capability,
    required String pointId,
    required String action,
    String operatorName = 'viewer',
  }) async {
    this.operatorName = operatorName;
    final resp = await rpc('COMMAND_EXECUTE', {
      'device_id': deviceId,
      'capability': capability,
      'point_id': pointId,
      'action': action,
    });
    return CommandAck.fromPayload(
      _payload(resp),
      result: resp['result'] as String? ?? 'ERR',
    );
  }

  @override
  Future<CommandAck> accessOpen({String operatorName = 'viewer'}) async {
    this.operatorName = operatorName;
    final resp = await rpc('ACCESS_OPEN', {'device_id': 'TS-001', 'pulse_ms': 300});
    return CommandAck.fromPayload(
      _payload(resp),
      result: resp['result'] as String? ?? 'ERR',
    );
  }

  @override
  Future<CommandAck> accessLock({String operatorName = 'viewer'}) async {
    this.operatorName = operatorName;
    final resp = await rpc('ACCESS_LOCK', {'device_id': 'TS-001'});
    return CommandAck.fromPayload(
      _payload(resp),
      result: resp['result'] as String? ?? 'ERR',
    );
  }

  @override
  Future<AccessStatus> accessStatus({String operatorName = 'viewer'}) async {
    this.operatorName = operatorName;
    final resp = await rpc('ACCESS_STATUS', {});
    return AccessStatus.fromJson(_payload(resp));
  }

  @override
  Future<AccessUsersPage> accessGetUsers({
    String operatorName = 'viewer',
    int offset = 0,
    int limit = 16,
  }) async {
    this.operatorName = operatorName;
    final resp = await rpc('ACCESS_GET_USERS', {
      'offset': offset,
      'limit': limit,
    });
    final payload = _payload(resp);
    final err = resp['error'] as String? ?? payload['error'] as String?;
    if (err == 'NO_PERMISSION' || err == 'BAD_AUTH') {
      return AccessUsersPage(users: [], error: err);
    }
    final rows = payload['users'] as List<dynamic>? ?? const [];
    return AccessUsersPage(
      users: rows
          .whereType<Map>()
          .map((e) => AccessUser.fromJson(Map<String, dynamic>.from(e)))
          .toList(),
      total: (payload['total'] as num?)?.toInt() ?? rows.length,
      offset: (payload['offset'] as num?)?.toInt() ?? offset,
      error: err,
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
    this.operatorName = operatorName;
    final payload = <String, dynamic>{
      'uid': canonicalizeUid(uid),
      'enabled': enabled,
    };
    if (employeeId != null && employeeId.isNotEmpty) {
      payload['employee_id'] = employeeId;
    }
    if (name != null && name.isNotEmpty) payload['name'] = name;
    final resp = await rpc('ACCESS_ADD_CARD', payload);
    return CommandAck.fromPayload(
      _payload(resp),
      result: resp['result'] as String? ?? 'ERR',
    );
  }

  @override
  Future<CommandAck> accessRemoveCard({
    required String uid,
    String operatorName = 'viewer',
  }) async {
    this.operatorName = operatorName;
    final resp = await rpc('ACCESS_REMOVE_CARD', {'uid': canonicalizeUid(uid)});
    return CommandAck.fromPayload(
      _payload(resp),
      result: resp['result'] as String? ?? 'ERR',
    );
  }

  @override
  Future<List<MeshEvent>> accessGetEvents({String operatorName = 'viewer'}) async {
    this.operatorName = operatorName;
    final resp = _payload(await rpc('ACCESS_GET_EVENTS', {'since': '0'}));
    final rows = resp['events'] as List<dynamic>? ?? const [];
    return rows
        .whereType<Map>()
        .map((e) => MeshEvent.fromJson(Map<String, dynamic>.from(e)))
        .toList();
  }
}
