import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../models/mesh_event.dart';
import '../widgets/chrome.dart';

class EventsScreen extends StatefulWidget {
  const EventsScreen({super.key, required this.events});

  final List<MeshEvent> events;

  @override
  State<EventsScreen> createState() => _EventsScreenState();
}

class _EventsScreenState extends State<EventsScreen> {
  String? _device;
  String? _type;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final events = widget.events.where((e) {
      if (_device != null && e.deviceId != _device) return false;
      if (_type != null && e.type != _type) return false;
      return true;
    }).toList();
    final devices = widget.events.map((e) => e.deviceId).toSet().toList()..sort();
    final types = widget.events.map((e) => e.type).toSet().toList()..sort();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(8, 8, 8, 0),
          child: Row(
            children: [
              Expanded(
                child: DropdownButtonFormField<String?>(
                  initialValue: _device,
                  decoration: InputDecoration(
                    labelText: l10n.filterDevice,
                    isDense: true,
                  ),
                  items: [
                    DropdownMenuItem(value: null, child: Text(l10n.filterAll)),
                    for (final id in devices)
                      DropdownMenuItem(value: id, child: Text(id)),
                  ],
                  onChanged: (v) => setState(() => _device = v),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: DropdownButtonFormField<String?>(
                  initialValue: _type,
                  decoration: InputDecoration(
                    labelText: l10n.filterType,
                    isDense: true,
                  ),
                  items: [
                    DropdownMenuItem(value: null, child: Text(l10n.filterAll)),
                    for (final t in types)
                      DropdownMenuItem(value: t, child: Text(t)),
                  ],
                  onChanged: (v) => setState(() => _type = v),
                ),
              ),
            ],
          ),
        ),
        Expanded(
          child: events.isEmpty
              ? Center(child: Text(l10n.eventsEmpty))
              : ListView.separated(
                  padding: const EdgeInsets.all(8),
                  itemCount: events.length,
                  separatorBuilder: (_, _) => const Divider(height: 1),
                  itemBuilder: (context, i) {
                    final e = events[i];
                    return ListTile(
                      title: Text('${e.type} · ${e.deviceId}'),
                      subtitle: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'id ${e.id} · source ${e.source}'
                            '${e.state != null && e.state!.isNotEmpty ? ' · state ${e.state}' : ''}'
                            '${e.rfidUid != null ? ' · uid ${e.rfidUid}' : ''}',
                          ),
                          DualAckChip(meshOk: e.meshOk, deviceOk: e.deviceOk),
                        ],
                      ),
                      isThreeLine: true,
                    );
                  },
                ),
        ),
      ],
    );
  }
}
