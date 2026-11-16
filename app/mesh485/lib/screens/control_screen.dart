import 'package:flutter/material.dart';

import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/point.dart';
import '../widgets/chrome.dart';

/// Capability buttons + extra confirmation. Shows mesh ACK vs device ACK vs timeout.
class ControlScreen extends StatefulWidget {
  const ControlScreen({
    super.key,
    required this.session,
    required this.devices,
    required this.points,
    required this.role,
  });

  final ConsoleSession session;
  final List<DeviceRecord> devices;
  final List<PointRecord> points;
  final String role;

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  CommandAck? _last;
  bool _busy = false;

  bool get _canControl =>
      widget.role == 'operator' || widget.role == 'admin';

  List<(DeviceRecord, PointRecord)> get _relays {
    final out = <(DeviceRecord, PointRecord)>[];
    for (final d in widget.devices) {
      if (!d.capabilities.contains('CONTROL_RELAY')) continue;
      for (final p in widget.points) {
        if (p.deviceId == d.id && p.type == 'relay') {
          out.add((d, p));
        }
      }
    }
    return out;
  }

  Future<void> _confirmAndRun(DeviceRecord d, PointRecord p, String action) async {
    final l10n = AppLocalizations.of(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        return AlertDialog(
          key: const Key('control-confirm'),
          title: Text(l10n.controlConfirmTitle),
          content: Text(
            l10n.controlConfirmBody(d.id, 'CONTROL_RELAY', p.pointId, action),
          ),
          actions: [
            TextButton(
              key: const Key('control-cancel'),
              onPressed: () => Navigator.of(ctx).pop(false),
              child: Text(l10n.controlCancel),
            ),
            FilledButton(
              key: const Key('control-execute'),
              onPressed: () => Navigator.of(ctx).pop(true),
              child: Text(l10n.controlConfirm),
            ),
          ],
        );
      },
    );
    if (ok != true || !mounted) return;
    setState(() => _busy = true);
    final ack = await widget.session.commandExecute(
      deviceId: d.id,
      capability: 'CONTROL_RELAY',
      pointId: p.pointId,
      action: action,
    );
    if (!mounted) return;
    setState(() {
      _busy = false;
      _last = ack;
    });
  }

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final relays = _relays;
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        if (!_canControl)
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: Text(
              l10n.controlViewerDenied,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        Text(l10n.controlHint, style: Theme.of(context).textTheme.bodySmall),
        const SizedBox(height: 12),
        if (relays.isEmpty) Text(l10n.controlEmpty),
        for (final pair in relays)
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    '${pair.$1.id} · CONTROL_RELAY · ${pair.$2.pointId}',
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                  Text('${pair.$2.name} · ${pair.$2.state}'),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    children: [
                      FilledButton(
                        key: Key('control-on-${pair.$1.id}-${pair.$2.pointId}'),
                        onPressed: _canControl && !_busy
                            ? () => _confirmAndRun(pair.$1, pair.$2, 'ON')
                            : null,
                        child: Text(l10n.controlOn),
                      ),
                      FilledButton.tonal(
                        key: Key('control-off-${pair.$1.id}-${pair.$2.pointId}'),
                        onPressed: _canControl && !_busy
                            ? () => _confirmAndRun(pair.$1, pair.$2, 'OFF')
                            : null,
                        child: Text(l10n.controlOff),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        if (_last != null) ...[
          const SizedBox(height: 16),
          Text(l10n.controlLastResult, style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 8),
          DualAckChip(
            meshOk: _last!.meshOk,
            deviceOk: _last!.deviceOk,
            timeout: _last!.timedOut,
            showTimeout: true,
          ),
          if (_last!.error != null)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text('error: ${_last!.error}'),
            ),
        ],
        if (_busy) const Padding(
          padding: EdgeInsets.only(top: 16),
          child: Center(child: CircularProgressIndicator()),
        ),
      ],
    );
  }
}
