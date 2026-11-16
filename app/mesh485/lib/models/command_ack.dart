/// Dual ACK for COMMAND_EXECUTE: mesh vs device vs timeout.
class CommandAck {
  const CommandAck({
    required this.meshOk,
    required this.deviceOk,
    this.error,
    this.result = 'ERR',
    this.deviceId = '',
    this.capability = '',
    this.pointId = '',
  });

  final bool meshOk;
  final bool deviceOk;
  final String? error;
  final String result;
  final String deviceId;
  final String capability;
  final String pointId;

  bool get timedOut => error == 'TIMEOUT';
  bool get denied => error == 'NO_PERMISSION';
  bool get badAuth => error == 'BAD_AUTH';
  bool get ok => result == 'OK' && deviceOk;

  factory CommandAck.fromPayload(Map<String, dynamic> payload, {String? result}) {
    return CommandAck(
      meshOk: payload['mesh_ok'] as bool? ?? false,
      deviceOk: payload['device_ok'] as bool? ?? false,
      error: payload['error'] as String?,
      result: result ?? (payload['error'] == null ? 'OK' : 'ERR'),
      deviceId: payload['device_id'] as String? ?? '',
      capability: payload['capability'] as String? ?? '',
      pointId: payload['point_id'] as String? ?? '',
    );
  }

  static const deniedAck = CommandAck(
    meshOk: true,
    deviceOk: false,
    error: 'NO_PERMISSION',
    result: 'ERR',
  );

  static const badAuthAck = CommandAck(
    meshOk: true,
    deviceOk: false,
    error: 'BAD_AUTH',
    result: 'ERR',
  );
}
