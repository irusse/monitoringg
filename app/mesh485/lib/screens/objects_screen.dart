import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../models/device.dart';
import '../models/node_health.dart';
import '../widgets/chrome.dart';

class ObjectsScreen extends StatelessWidget {
  const ObjectsScreen({
    super.key,
    required this.objects,
    required this.devices,
  });

  final List<SiteObject> objects;
  final List<DeviceRecord> devices;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    if (objects.isEmpty) {
      return Center(child: Text(l10n.noObjects));
    }
    return ListView.builder(
      padding: const EdgeInsets.all(16),
      itemCount: objects.length,
      itemBuilder: (context, i) {
        final site = objects[i];
        return Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(site.name, style: Theme.of(context).textTheme.titleLarge),
                Text(site.id, style: Theme.of(context).textTheme.bodySmall),
                const SizedBox(height: 12),
                for (final id in site.deviceIds)
                  if (_byId(id) case final d?) _row(context, d),
                const SizedBox(height: 12),
                Text(l10n.objectsNote, style: Theme.of(context).textTheme.bodySmall),
              ],
            ),
          ),
        );
      },
    );
  }

  DeviceRecord? _byId(String id) {
    for (final d in devices) {
      if (d.id == id) return d;
    }
    return null;
  }

  Widget _row(BuildContext context, DeviceRecord d) {
    return ListTile(
      contentPadding: EdgeInsets.zero,
      leading: StatusDot(online: d.online),
      title: Text(d.id),
      subtitle: Text('${d.model} · ${d.protocol}'),
      trailing: Text(onlineLabel(context, d.online)),
    );
  }
}
