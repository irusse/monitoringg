import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../models/node_health.dart';
import '../widgets/chrome.dart';

/// Extender-node health. Not MeshCore contacts, chat, channels, or QR.
class MeshScreen extends StatelessWidget {
  const MeshScreen({super.key, required this.health});

  final NodeHealth health;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final rows = <(String, String)>[
      (l10n.meshNode, health.node),
      (l10n.meshRadio, health.radio),
      (l10n.meshRepeater, health.repeater ? l10n.on : l10n.off),
      (l10n.meshRssi, '${health.rssi} dBm'),
      (l10n.meshSnr, '${health.snr} dB'),
      (l10n.meshRx, '${health.rx}'),
      (l10n.meshTx, '${health.tx}'),
      (l10n.meshUptime, l10n.seconds(health.uptimeS)),
      (l10n.meshLastSeen, health.lastSeen),
      (l10n.meshProfile, health.profile),
      (l10n.meshAdapters, '${health.devices}'),
      (l10n.meshRs485, '${health.rs485Baud} ${health.rs485Protocol}'),
    ];
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          l10n.meshTitle,
          style: Theme.of(context).textTheme.titleMedium,
        ),
        const SizedBox(height: 12),
        DualAckChip(meshOk: health.meshOk, deviceOk: health.deviceOk),
        const SizedBox(height: 8),
        for (final row in rows)
          ListTile(
            dense: true,
            title: Text(row.$1),
            trailing: Text(row.$2),
          ),
      ],
    );
  }
}
