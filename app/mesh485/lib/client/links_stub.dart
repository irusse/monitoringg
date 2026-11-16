import 'live_client.dart';

/// Platform links. Stub when serial/BLE plugins are absent (web / tests).
List<String> listSerialPorts() => const [];

Future<Mesh485Link?> openSerialPort(String port) async => null;

class BleTarget {
  const BleTarget(this.id, this.name);
  final String id;
  final String name;
}

Future<List<BleTarget>> scanBleNodes({
  Duration timeout = const Duration(seconds: 4),
}) async =>
    const [];

Future<Mesh485Link?> openBleNode(BleTarget target) async => null;
