import 'dart:async';

import 'package:flutter/foundation.dart';

import '../access/photo_store.dart';
import '../models/access_user.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/mesh_event.dart';
import '../models/node_health.dart';
import '../models/point.dart';
import 'fixture_client.dart';
import 'links.dart';
import 'live_client.dart';
import 'mesh485_client.dart';

/// Picks live USB/BLE or fixture fallback. Settings shows [transportLabel].
class ConsoleSession extends ChangeNotifier {
  ConsoleSession({Mesh485Client? forced})
      : client = forced ?? FixtureMesh485Client(),
        _locked = forced != null;

  Mesh485Client client;
  final PhotoStore photos = PhotoStore();
  final bool _locked;
  bool usingFixture = true;
  String? lastError;
  ConsoleSnapshot? snapshot;
  List<String> serialPorts = const [];
  List<BleTarget> bleTargets = const [];
  String selectedKey = 'fixture';
  String role = 'viewer';

  bool get canControl => role == 'operator' || role == 'admin';

  String get transportLabel => client.transportLabel;

  Future<void> refreshPorts() async {
    serialPorts = listSerialPorts();
    bleTargets = await scanBleNodes();
    notifyListeners();
  }

  Future<void> boot() async {
    await refreshPorts();
    if (_locked) {
      await reload();
      return;
    }
    final preferred = _preferredLive();
    if (preferred != null) {
      await connectKey(preferred);
    } else {
      await useFixture(reason: null);
    }
  }

  String? _preferredLive() {
    if (serialPorts.contains('COM23')) return 'usb:COM23';
    if (serialPorts.isNotEmpty) return 'usb:${serialPorts.first}';
    for (final b in bleTargets) {
      if (b.name == '3MESH-TS-001') return 'ble:${b.id}';
    }
    if (bleTargets.isNotEmpty) return 'ble:${bleTargets.first.id}';
    return null;
  }

  Future<void> useFixture({String? reason}) async {
    lastError = reason;
    selectedKey = 'fixture';
    usingFixture = true;
    if (client is LiveMesh485Client) {
      await (client as LiveMesh485Client).shutdown();
    }
    client = FixtureMesh485Client();
    await reload();
  }

  Future<void> connectKey(String key) async {
    if (_locked) return;
    if (key == 'fixture') {
      await useFixture();
      return;
    }
    Mesh485Link? link;
    try {
      if (key.startsWith('usb:')) {
        link = await openSerialPort(key.substring(4));
      } else if (key.startsWith('ble:')) {
        final id = key.substring(4);
        BleTarget? t;
        for (final b in bleTargets) {
          if (b.id == id) t = b;
        }
        t ??= BleTarget(id, id);
        link = await openBleNode(t);
      }
      if (link == null) {
        await useFixture(reason: 'no link');
        return;
      }
      if (client is LiveMesh485Client) {
        await (client as LiveMesh485Client).shutdown();
      }
      final live = LiveMesh485Client(link, operatorName: role);
      await live.ensureOpen();
      client = live;
      selectedKey = key;
      usingFixture = false;
      lastError = null;
      await reload();
    } catch (e) {
      await useFixture(reason: '$e');
    }
  }

  Future<void> reload() async {
    try {
      final devices = await client.discoverDevices();
      final points = await client.pointList();
      final events = await client.eventList();
      final health = await client.nodeStatus();
      final bridge = await client.bridgeStatus();
      final objects = await client.listObjects();
      snapshot = ConsoleSnapshot(
        devices: devices,
        points: points,
        events: events,
        health: health,
        bridge: bridge,
        objects: objects,
      );
    } catch (e) {
      lastError = '$e';
      if (!usingFixture && !_locked) {
        await useFixture(reason: '$e');
        return;
      }
    }
    notifyListeners();
  }

  /// [next] is a factory allow-list name: `viewer` / `operator` / `admin`.
  void setRole(String next) {
    role = next;
    if (client is LiveMesh485Client) {
      (client as LiveMesh485Client).operatorName = next;
    }
    notifyListeners();
  }

  Future<CommandAck> accessOpen({String? operatorName}) {
    return client.accessOpen(operatorName: operatorName ?? role);
  }

  Future<CommandAck> accessLock({String? operatorName}) {
    return client.accessLock(operatorName: operatorName ?? role);
  }

  Future<AccessStatus> accessStatus({String? operatorName}) {
    return client.accessStatus(operatorName: operatorName ?? role);
  }

  Future<List<AccessUser>> accessGetUsers({String? operatorName}) async {
    final op = operatorName ?? role;
    final all = <AccessUser>[];
    var offset = 0;
    while (true) {
      final page = await client.accessGetUsers(
        operatorName: op,
        offset: offset,
        limit: 16,
      );
      if (page.denied) return all;
      all.addAll(page.users);
      if (all.length >= page.total || page.users.isEmpty) break;
      offset += page.users.length;
      if (offset >= 64) break;
    }
    return all;
  }

  Future<CommandAck> accessAddCard({
    required String uid,
    String? employeeId,
    String? name,
    bool enabled = true,
    String? operatorName,
  }) {
    return client.accessAddCard(
      uid: uid,
      employeeId: employeeId,
      name: name,
      enabled: enabled,
      operatorName: operatorName ?? role,
    );
  }

  Future<CommandAck> accessRemoveCard({required String uid, String? operatorName}) {
    return client.accessRemoveCard(uid: uid, operatorName: operatorName ?? role);
  }

  Future<List<MeshEvent>> accessGetEvents({String? operatorName}) {
    return client.accessGetEvents(operatorName: operatorName ?? role);
  }

  Future<CommandAck> commandExecute({
    required String deviceId,
    required String capability,
    required String pointId,
    required String action,
  }) {
    return client.commandExecute(
      deviceId: deviceId,
      capability: capability,
      pointId: pointId,
      action: action,
      operatorName: role,
    );
  }

  @override
  void dispose() {
    if (client is LiveMesh485Client) {
      unawaited((client as LiveMesh485Client).shutdown());
    }
    super.dispose();
  }
}

class ConsoleSnapshot {
  const ConsoleSnapshot({
    required this.devices,
    required this.points,
    required this.events,
    required this.health,
    required this.bridge,
    required this.objects,
  });

  final List<DeviceRecord> devices;
  final List<PointRecord> points;
  final List<MeshEvent> events;
  final NodeHealth health;
  final BridgeStatus bridge;
  final List<SiteObject> objects;
}
