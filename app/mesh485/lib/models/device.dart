/// MESH 485 Device record. Shape matches `docs/device-adapters.md`.
class DeviceRecord {
  const DeviceRecord({
    required this.id,
    required this.vendor,
    required this.model,
    required this.protocol,
    required this.address,
    required this.online,
    required this.capabilities,
    this.state,
  });

  final String id;
  final String vendor;
  final String model;
  final String protocol;
  final int address;
  final bool online;
  final List<String> capabilities;
  /// `OK` / `OFFLINE` from DEVICE_STATUS when present.
  final String? state;

  factory DeviceRecord.fromJson(Map<String, dynamic> json) {
    return DeviceRecord(
      id: json['id'] as String,
      vendor: json['vendor'] as String? ?? '',
      model: json['model'] as String? ?? '',
      protocol: json['protocol'] as String? ?? '',
      address: (json['address'] as num?)?.toInt() ?? 0,
      online: json['online'] as bool? ?? false,
      capabilities: (json['capabilities'] as List<dynamic>? ?? const [])
          .map((e) => e.toString())
          .toList(),
      state: json['state'] as String?,
    );
  }

  String get statusLabel => online ? (state ?? 'OK') : (state ?? 'OFFLINE');
}
