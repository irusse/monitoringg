import 'package:flutter/material.dart';

import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../widgets/chrome.dart';

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({
    super.key,
    required this.session,
    required this.role,
  });

  final ConsoleSession session;
  final String role;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final health = session.snapshot?.health;
    final keys = <String>['fixture'];
    for (final p in session.serialPorts) {
      keys.add('usb:$p');
    }
    for (final b in session.bleTargets) {
      keys.add('ble:${b.id}');
    }
    if (!keys.contains(session.selectedKey)) {
      keys.add(session.selectedKey);
    }
    String labelFor(String key) {
      if (key == 'fixture') return l10n.settingsFixture;
      if (key.startsWith('usb:')) return key;
      if (key.startsWith('ble:')) {
        final id = key.substring(4);
        for (final b in session.bleTargets) {
          if (b.id == id) return 'ble:${b.name}';
        }
        return key;
      }
      return key;
    }

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        ListTile(
          title: Text(l10n.settingsRole),
          trailing: RoleChip(role: role),
        ),
        ListTile(
          title: Text(l10n.settingsTransport),
          subtitle: Text(session.transportLabel),
        ),
        if (session.lastError != null)
          ListTile(
            title: Text(l10n.connectFailed),
            subtitle: Text(session.lastError!),
          ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: DropdownButtonFormField<String>(
            initialValue: session.selectedKey,
            decoration: InputDecoration(labelText: l10n.settingsPortPicker),
            items: [
              for (final k in keys)
                DropdownMenuItem(value: k, child: Text(labelFor(k))),
            ],
            onChanged: (v) {
              if (v != null) session.connectKey(v);
            },
          ),
        ),
        ListTile(
          title: Text(l10n.settingsRefresh),
          trailing: IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () async {
              await session.refreshPorts();
              await session.reload();
            },
          ),
        ),
        ListTile(
          title: Text(l10n.settingsServer),
          subtitle: Text(l10n.settingsServerNone),
        ),
        if (health != null) ...[
          ListTile(
            title: Text(l10n.settingsNode),
            trailing: Text(health.node),
          ),
          ListTile(
            title: Text(l10n.settingsPort),
            subtitle: Text(
              l10n.settingsPortHint(health.rs485Protocol, health.rs485Baud),
            ),
          ),
        ],
        ListTile(
          title: Text(l10n.settingsBlePin),
          subtitle: Text(l10n.settingsBleGap),
        ),
      ],
    );
  }
}
