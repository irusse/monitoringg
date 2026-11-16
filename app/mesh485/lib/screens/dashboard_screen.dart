import 'package:flutter/material.dart';

import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../widgets/chrome.dart';

class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key, required this.data});

  final ConsoleSnapshot data;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final fault = data.points.where((p) => p.isFault).length;
    final alarm = data.points.where((p) => p.isAlarm).length;
    final normal = data.points.where((p) => p.state == 'NORMAL').length;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text('${l10n.statExtender} ${data.health.node}',
            style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _stat(context, l10n.statObjects, '${data.objects.length}'),
            _stat(context, l10n.statDevices, '${data.devices.length}'),
            _stat(context, l10n.statNormal, '$normal'),
            _stat(context, l10n.statFault, '$fault'),
            _stat(context, l10n.statAlarm, '$alarm'),
            _stat(
              context,
              l10n.statExtender,
              data.health.extenderOnline ? l10n.online : l10n.offline,
            ),
          ],
        ),
        const SizedBox(height: 16),
        DualAckChip(
          meshOk: data.health.meshOk,
          deviceOk: data.health.deviceOk,
        ),
        const SizedBox(height: 8),
        Text(
          l10n.dashboardHint,
          style: Theme.of(context).textTheme.bodySmall,
        ),
      ],
    );
  }

  Widget _stat(BuildContext context, String label, String value) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label, style: Theme.of(context).textTheme.labelMedium),
            Text(value, style: Theme.of(context).textTheme.headlineSmall),
          ],
        ),
      ),
    );
  }
}
