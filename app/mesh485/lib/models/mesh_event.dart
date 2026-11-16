/// MESH 485 Event JSON. Dual ACK is [meshOk] vs [deviceOk] — not one bool.
class MeshEvent {
  const MeshEvent({
    required this.id,
    required this.deviceId,
    required this.type,
    required this.source,
    required this.meshOk,
    required this.deviceOk,
    this.pointId,
    this.severity,
    this.state,
    this.timestamp = 0,
    this.sequence = 0,
    this.rfidUid,
    this.action,
    this.status,
  });

  final String id;
  final String deviceId;
  final String? pointId;
  final String type;
  final String? severity;
  final String? state;
  final int timestamp;
  final int sequence;
  final String source;
  final bool meshOk;
  final bool deviceOk;
  final String? rfidUid;
  final String? action;
  final String? status;

  factory MeshEvent.fromJson(Map<String, dynamic> json) {
    return MeshEvent(
      id: json['id'] as String,
      deviceId: json['device_id'] as String,
      pointId: json['point_id'] as String?,
      type: json['type'] as String,
      severity: json['severity'] as String?,
      state: json['state'] as String?,
      timestamp: (json['timestamp'] as num?)?.toInt() ?? 0,
      sequence: (json['sequence'] as num?)?.toInt() ?? 0,
      source: json['source'] as String? ?? '',
      meshOk: json['mesh_ok'] as bool? ?? false,
      deviceOk: json['device_ok'] as bool? ?? false,
      rfidUid: json['rfid_uid'] as String?,
      action: json['action'] as String?,
      status: json['status'] as String?,
    );
  }
}
