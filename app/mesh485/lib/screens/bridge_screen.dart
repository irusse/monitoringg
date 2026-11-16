import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../models/node_health.dart';

/// Bridge placeholder. Default READ_ONLY. No operator BRIDGE_OPEN.
class BridgeScreen extends StatelessWidget {
  const BridgeScreen({super.key, required this.bridge});

  final BridgeStatus bridge;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        ListTile(
          title: Text(l10n.bridgeSession),
          trailing: Text(bridge.session),
        ),
        ListTile(
          title: Text(l10n.bridgeLevel),
          trailing: Text(bridge.level),
        ),
        ListTile(
          title: Text(l10n.bridgePort),
          trailing: Text(bridge.port),
        ),
        const SizedBox(height: 8),
        FilledButton.tonal(
          onPressed: null,
          child: Text(l10n.bridgeOpenLater),
        ),
        const SizedBox(height: 12),
        Text(
          l10n.bridgeHint,
          style: Theme.of(context).textTheme.bodySmall,
        ),
      ],
    );
  }
}
