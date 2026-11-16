import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';

/// Operator role chip (VIEWER / OPERATOR / ADMIN).
class RoleChip extends StatelessWidget {
  const RoleChip({super.key, required this.role});

  final String role;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final label = switch (role) {
      'operator' => l10n.roleOperator,
      'admin' => l10n.roleAdmin,
      _ => l10n.roleViewer,
    };
    return Chip(
      visualDensity: VisualDensity.compact,
      avatar: const Icon(Icons.person_outline, size: 18),
      label: Text(label),
    );
  }
}

class DualAckChip extends StatelessWidget {
  const DualAckChip({
    super.key,
    required this.meshOk,
    required this.deviceOk,
    this.timeout = false,
    this.showTimeout = false,
  });

  final bool meshOk;
  final bool deviceOk;
  final bool timeout;
  final bool showTimeout;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return Wrap(
      spacing: 8,
      runSpacing: 4,
      children: [
        _flag('${l10n.ackMesh} mesh_ok', meshOk),
        _flag('${l10n.ackDevice} device_ok', deviceOk),
        if (showTimeout)
          _flag(
            timeout ? '${l10n.ackTimeout} TIMEOUT' : l10n.ackTimeout,
            timeout,
          ),
      ],
    );
  }

  Widget _flag(String name, bool ok) {
    return Chip(
      visualDensity: VisualDensity.compact,
      avatar: Icon(
        ok ? Icons.check_circle_outline : Icons.highlight_off,
        size: 18,
        color: ok ? Colors.green.shade700 : Colors.orange.shade800,
      ),
      label: Text('$name: $ok'),
    );
  }
}

class StatusDot extends StatelessWidget {
  const StatusDot({super.key, required this.online});

  final bool online;

  @override
  Widget build(BuildContext context) {
    return Icon(
      Icons.circle,
      size: 12,
      color: online ? Colors.green.shade600 : Colors.grey,
    );
  }
}

String onlineLabel(BuildContext context, bool online) {
  final l10n = AppLocalizations.of(context);
  return online ? l10n.online : l10n.offline;
}
