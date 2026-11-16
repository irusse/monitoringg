/// Extender-node health (Mesh screen). Not MeshCore contacts/chat.
class NodeHealth {
  const NodeHealth({
    required this.node,
    required this.radio,
    required this.repeater,
    required this.uptimeS,
    required this.profile,
    required this.devices,
    required this.rssi,
    required this.snr,
    required this.rx,
    required this.tx,
    required this.lastSeen,
    required this.meshOk,
    required this.deviceOk,
    this.rs485Ok = true,
    this.rs485Baud = 9600,
    this.rs485Protocol = 'RAW',
    this.session = 'off',
    this.queued = 0,
    this.pendingMesh = 0,
  });

  final String node;
  final String radio;
  final bool repeater;
  final int uptimeS;
  final String profile;
  final int devices;
  final int rssi;
  final double snr;
  final int rx;
  final int tx;
  final String lastSeen;
  final bool meshOk;
  final bool deviceOk;
  final bool rs485Ok;
  final int rs485Baud;
  final String rs485Protocol;
  final String session;
  final int queued;
  final int pendingMesh;

  bool get extenderOnline => radio == 'ok';
}

/// Placeholder bridge session. Live BRIDGE_OPEN is prompt 13.
class BridgeStatus {
  const BridgeStatus({
    required this.session,
    required this.level,
    required this.port,
  });

  /// `off` or an active level name.
  final String session;
  /// Product default is READ_ONLY even when session is off.
  final String level;
  final String port;

  bool get isOpen => session != 'off';
}

/// One site card on Objects.
class SiteObject {
  const SiteObject({
    required this.id,
    required this.name,
    required this.deviceIds,
    this.note,
  });

  final String id;
  final String name;
  final List<String> deviceIds;
  final String? note;
}
