import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:serial_port_win32/serial_port_win32.dart';

import 'live_client.dart';

class BleTarget {
  const BleTarget(this.id, this.name);
  final String id;
  final String name;
}

List<String> listSerialPorts() {
  if (!Platform.isWindows) return const [];
  try {
    return SerialPort.getAvailablePorts();
  } catch (_) {
    return const [];
  }
}

Future<Mesh485Link?> openSerialPort(String port) async {
  if (!Platform.isWindows) return null;
  return _SerialLink(port);
}

class _SerialLink implements Mesh485Link {
  _SerialLink(this._portName);

  final String _portName;
  SerialPort? _port;
  final _lines = StreamController<String>.broadcast();
  final _buf = StringBuffer();
  Timer? _poll;
  bool _opened = false;
  bool _busy = false;

  @override
  String get label => 'usb:$_portName';

  @override
  Future<void> open() async {
    if (_opened) return;
    final p = SerialPort(_portName, openNow: false, BaudRate: 115200);
    await p.open();
    _port = p;
    // Opening CDC often resets the S3 — same wait as m485_devices.py.
    await Future<void>.delayed(const Duration(milliseconds: 2500));
    try {
      await p.readBytes(4096, timeout: Duration.zero);
    } catch (_) {}
    _poll = Timer.periodic(const Duration(milliseconds: 40), (_) {
      unawaited(_drain());
    });
    _opened = true;
  }

  Future<void> _drain() async {
    if (_busy) return;
    final p = _port;
    if (p == null || !p.isOpened) return;
    _busy = true;
    try {
      final chunk = await p.readBytes(512, timeout: const Duration(milliseconds: 1));
      if (chunk.isEmpty) return;
      _buf.write(utf8.decode(chunk, allowMalformed: true));
      var s = _buf.toString();
      var idx = s.indexOf('\n');
      while (idx >= 0) {
        final line = s.substring(0, idx);
        s = s.substring(idx + 1);
        if (!_lines.isClosed) _lines.add(line);
        idx = s.indexOf('\n');
      }
      _buf
        ..clear()
        ..write(s);
    } catch (_) {
    } finally {
      _busy = false;
    }
  }

  @override
  Future<void> writeLine(String line) async {
    final p = _port;
    if (p == null) throw StateError('serial closed');
    await p.writeBytesFromString(
      '$line\n',
      includeZeroTerminator: false,
    );
  }

  @override
  Stream<String> get lines => _lines.stream;

  @override
  Future<void> close() async {
    _poll?.cancel();
    _poll = null;
    try {
      _port?.close();
    } catch (_) {}
    _port = null;
    _opened = false;
  }
}

const _nusService = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E';
const _nusRx = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E';
const _nusTx = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';

Future<List<BleTarget>> scanBleNodes({
  Duration timeout = const Duration(seconds: 4),
}) async {
  if (!Platform.isAndroid) return const [];
  try {
    final adapterOn = await FlutterBluePlus.isSupported;
    if (!adapterOn) return const [];
    await FlutterBluePlus.startScan(timeout: timeout);
    await Future<void>.delayed(timeout);
    await FlutterBluePlus.stopScan();
    final out = <BleTarget>[];
    for (final r in FlutterBluePlus.lastScanResults) {
      final name = r.advertisementData.advName;
      if (name.startsWith('3MESH-')) {
        out.add(BleTarget(r.device.remoteId.str, name));
      }
    }
    return out;
  } catch (_) {
    return const [];
  }
}

Future<Mesh485Link?> openBleNode(BleTarget target) async {
  if (!Platform.isAndroid) return null;
  return _BleLink(target);
}

class _BleLink implements Mesh485Link {
  _BleLink(this._target);

  final BleTarget _target;
  BluetoothDevice? _dev;
  BluetoothCharacteristic? _rx;
  final _lines = StreamController<String>.broadcast();
  final _buf = StringBuffer();
  StreamSubscription<List<int>>? _notify;

  @override
  String get label => 'ble:${_target.name}';

  @override
  Future<void> open() async {
    final dev = BluetoothDevice.fromId(_target.id);
    _dev = dev;
    await dev.connect(timeout: const Duration(seconds: 12));
    await Future<void>.delayed(const Duration(milliseconds: 400));
    final services = await dev.discoverServices();
    BluetoothCharacteristic? tx;
    for (final s in services) {
      final sid = s.uuid.toString().toUpperCase().replaceAll('-', '');
      if (sid != _nusService.replaceAll('-', '')) continue;
      for (final c in s.characteristics) {
        final id = c.uuid.toString().toUpperCase().replaceAll('-', '');
        if (id == _nusRx.replaceAll('-', '')) _rx = c;
        if (id == _nusTx.replaceAll('-', '')) tx = c;
      }
    }
    if (_rx == null || tx == null) {
      throw StateError('NUS not found on ${_target.name}');
    }
    await tx.setNotifyValue(true);
    _notify = tx.onValueReceived.listen((bytes) {
      _buf.write(utf8.decode(bytes, allowMalformed: true));
      var s = _buf.toString();
      var idx = s.indexOf('\n');
      while (idx >= 0) {
        final line = s.substring(0, idx);
        s = s.substring(idx + 1);
        if (!_lines.isClosed) _lines.add(line);
        idx = s.indexOf('\n');
      }
      _buf
        ..clear()
        ..write(s);
    });
  }

  @override
  Future<void> writeLine(String line) async {
    final rx = _rx;
    if (rx == null) throw StateError('ble closed');
    final data = utf8.encode('$line\n');
    // Chunk to NUS MTU; firmware ble mux concatenates until \n.
    const chunk = 160;
    for (var i = 0; i < data.length; i += chunk) {
      final end = i + chunk > data.length ? data.length : i + chunk;
      await rx.write(data.sublist(i, end), withoutResponse: false);
    }
  }

  @override
  Stream<String> get lines => _lines.stream;

  @override
  Future<void> close() async {
    await _notify?.cancel();
    _notify = null;
    try {
      await _dev?.disconnect();
    } catch (_) {}
    _dev = null;
    _rx = null;
  }
}
