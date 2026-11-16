import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../models/device.dart';
import '../models/point.dart';
import '../widgets/chrome.dart';

class DevicesScreen extends StatelessWidget {
  const DevicesScreen({
    super.key,
    required this.devices,
    required this.points,
  });

  final List<DeviceRecord> devices;
  final List<PointRecord> points;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return ListView.builder(
      padding: const EdgeInsets.all(8),
      itemCount: devices.length,
      itemBuilder: (context, i) {
        final d = devices[i];
        final pts = points.where((p) => p.deviceId == d.id).toList();
        return Card(
          child: ExpansionTile(
            leading: StatusDot(online: d.online),
            title: Text(d.id),
            subtitle: Text(
              '${d.vendor} · ${d.model} · ${d.protocol} · '
              '${onlineLabel(context, d.online)}',
            ),
            children: [
              ListTile(
                dense: true,
                title: Text('${l10n.capabilities}: ${d.capabilities.join(', ')}'),
                subtitle: Text('${l10n.address} ${d.address} · ${d.statusLabel}'),
              ),
              for (final p in pts)
                ListTile(
                  dense: true,
                  leading: const Icon(Icons.linear_scale, size: 20),
                  title: Text('${p.pointId} · ${p.name}'),
                  subtitle: Text('${p.type} · ${p.state}'),
                ),
            ],
          ),
        );
      },
    );
  }
}
