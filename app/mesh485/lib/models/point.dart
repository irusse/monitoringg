/// MESH 485 Point record.
class PointRecord {
  const PointRecord({
    required this.deviceId,
    required this.pointId,
    required this.type,
    required this.name,
    required this.state,
    this.attributes = const {},
  });

  final String deviceId;
  final String pointId;
  final String type;
  final String name;
  final String state;
  final Map<String, dynamic> attributes;

  factory PointRecord.fromJson(Map<String, dynamic> json) {
    return PointRecord(
      deviceId: json['device_id'] as String,
      pointId: json['point_id'] as String,
      type: json['type'] as String? ?? '',
      name: json['name'] as String? ?? '',
      state: json['state'] as String? ?? 'UNKNOWN',
      attributes: Map<String, dynamic>.from(
        json['attributes'] as Map? ?? const {},
      ),
    );
  }

  bool get isFault => state == 'FAULT';
  bool get isAlarm => state == 'ALARM';
}
