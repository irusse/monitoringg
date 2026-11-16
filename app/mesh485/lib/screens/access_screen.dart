import 'package:flutter/material.dart';

import '../access/photo_view.dart';
import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../models/access_user.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/mesh_event.dart';
import '../models/point.dart';
import '../widgets/chrome.dart';

/// TS-001 Access: employees/cards (ADMIN), disk photos, OPEN+LOCK, live events.
class AccessScreen extends StatefulWidget {
  const AccessScreen({
    super.key,
    required this.session,
    required this.devices,
    required this.points,
    required this.events,
    required this.role,
  });

  final ConsoleSession session;
  final List<DeviceRecord> devices;
  final List<PointRecord> points;
  final List<MeshEvent> events;
  final String role;

  @override
  State<AccessScreen> createState() => _AccessScreenState();
}

class _AccessScreenState extends State<AccessScreen> {
  CommandAck? _last;
  AccessStatus? _status;
  List<AccessUser> _users = const [];
  List<MeshEvent> _accessEvents = const [];
  bool _busy = false;
  String? _note;

  bool get _canOpen => widget.role == 'operator' || widget.role == 'admin';
  bool get _isAdmin => widget.role == 'admin';

  @override
  void initState() {
    super.initState();
    _refresh();
  }

  @override
  void didUpdateWidget(covariant AccessScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.role != widget.role) {
      _refresh();
    }
  }

  Future<void> _refresh() async {
    setState(() => _busy = true);
    AccessStatus? status;
    var users = const <AccessUser>[];
    var events = const <MeshEvent>[];
    try {
      status = await widget.session.accessStatus(operatorName: widget.role);
      events = await widget.session.accessGetEvents(operatorName: widget.role);
      if (_isAdmin) {
        users = await widget.session.accessGetUsers(operatorName: widget.role);
      }
    } catch (e) {
      _note = '$e';
    }
    if (!mounted) return;
    setState(() {
      _busy = false;
      _status = status;
      _users = users;
      _accessEvents = events.isNotEmpty
          ? events
          : widget.events.where((e) => e.deviceId == 'TS-001').toList();
    });
  }

  Future<void> _confirmOp({
    required Key dialogKey,
    required Key executeKey,
    required String title,
    required String body,
    required Future<CommandAck> Function() run,
  }) async {
    final l10n = AppLocalizations.of(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        return AlertDialog(
          key: dialogKey,
          title: Text(title),
          content: Text(body),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: Text(l10n.controlCancel),
            ),
            FilledButton(
              key: executeKey,
              onPressed: () => Navigator.of(ctx).pop(true),
              child: Text(l10n.controlConfirm),
            ),
          ],
        );
      },
    );
    if (ok != true || !mounted) return;
    setState(() => _busy = true);
    final ack = await run();
    if (!mounted) return;
    setState(() {
      _busy = false;
      _last = ack;
      _note = ack.error;
    });
  }

  Future<void> _confirmAndOpen() async {
    final l10n = AppLocalizations.of(context);
    await _confirmOp(
      dialogKey: const Key('access-confirm'),
      executeKey: const Key('access-execute'),
      title: l10n.accessConfirmTitle,
      body: l10n.accessConfirmBody,
      run: () => widget.session.accessOpen(operatorName: widget.role),
    );
  }

  Future<void> _confirmAndLock() async {
    final l10n = AppLocalizations.of(context);
    await _confirmOp(
      dialogKey: const Key('access-lock-confirm'),
      executeKey: const Key('access-lock-execute'),
      title: l10n.accessLockConfirmTitle,
      body: l10n.accessLockConfirmBody,
      run: () => widget.session.accessLock(operatorName: widget.role),
    );
  }

  Future<void> _editUser(AccessUser? existing) async {
    final l10n = AppLocalizations.of(context);
    final nameCtrl = TextEditingController(text: existing?.name ?? '');
    final idCtrl = TextEditingController(text: existing?.employeeId ?? '');
    final uidCtrl = TextEditingController(text: existing?.uid ?? '');
    final photoCtrl = TextEditingController(
      text: existing == null
          ? ''
          : (widget.session.photos.pathFor(existing.photoKey) ?? ''),
    );
    var enabled = existing?.enabled ?? true;
    final saved = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        return StatefulBuilder(
          builder: (ctx, setLocal) {
            return AlertDialog(
              key: const Key('access-employee-dialog'),
              title: Text(
                existing == null ? l10n.accessAddEmployee : l10n.accessEditEmployee,
              ),
              content: SizedBox(
                width: 420,
                child: SingleChildScrollView(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      TextField(
                        key: const Key('access-employee-name'),
                        controller: nameCtrl,
                        decoration: InputDecoration(labelText: l10n.accessName),
                      ),
                      TextField(
                        key: const Key('access-employee-id'),
                        controller: idCtrl,
                        decoration: InputDecoration(labelText: l10n.accessEmployeeId),
                      ),
                      TextField(
                        key: const Key('access-uid'),
                        controller: uidCtrl,
                        decoration: InputDecoration(labelText: l10n.accessUid),
                      ),
                      SwitchListTile(
                        key: const Key('access-enabled'),
                        contentPadding: EdgeInsets.zero,
                        title: Text(l10n.accessEnabled),
                        value: enabled,
                        onChanged: (v) => setLocal(() => enabled = v),
                      ),
                      TextField(
                        key: const Key('access-photo'),
                        controller: photoCtrl,
                        decoration: InputDecoration(
                          labelText: l10n.accessPhotoPath,
                          helperText: l10n.accessPhotoHint,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.of(ctx).pop(false),
                  child: Text(l10n.controlCancel),
                ),
                if (existing != null)
                  TextButton(
                    key: const Key('access-remove'),
                    onPressed: () => Navigator.of(ctx).pop(null),
                    child: Text(l10n.accessRemove),
                  ),
                FilledButton(
                  key: const Key('access-save'),
                  onPressed: () => Navigator.of(ctx).pop(true),
                  child: Text(l10n.accessSave),
                ),
              ],
            );
          },
        );
      },
    );
    if (!mounted) return;
    final name = nameCtrl.text.trim();
    final empId = idCtrl.text.trim();
    final uid = canonicalizeUid(uidCtrl.text);
    final photo = photoCtrl.text.trim();
    if (saved == null && existing != null) {
      setState(() => _busy = true);
      final ack = await widget.session.accessRemoveCard(
        uid: existing.uid,
        operatorName: widget.role,
      );
      if (!mounted) return;
      setState(() {
        _busy = false;
        _last = ack;
        _note = ack.error;
      });
      await _refresh();
      return;
    }
    if (saved != true) return;
    if (uid.isEmpty) {
      setState(() => _note = l10n.accessUidRequired);
      return;
    }
    setState(() => _busy = true);
    final ack = await widget.session.accessAddCard(
      uid: uid,
      employeeId: empId.isEmpty ? null : empId,
      name: name.isEmpty ? null : name,
      enabled: enabled,
      operatorName: widget.role,
    );
    if (ack.result == 'OK' || ack.ok) {
      widget.session.photos.setPath(empId.isNotEmpty ? empId : uid, photo);
    }
    if (!mounted) return;
    setState(() {
      _busy = false;
      _last = ack;
      _note = ack.error;
    });
    await _refresh();
  }

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    DeviceRecord? match;
    for (final d in widget.devices) {
      if (d.id == 'TS-001') {
        match = d;
        break;
      }
    }
    final ts = match;
    final pts = widget.points.where((p) => p.deviceId == 'TS-001').toList();
    if (ts == null) {
      return Center(child: Text(l10n.accessMissing));
    }
    final byId = <String, MeshEvent>{};
    for (final e in widget.events.where((e) => e.deviceId == 'TS-001')) {
      byId[e.id] = e;
    }
    for (final e in _accessEvents) {
      byId[e.id] = e;
    }
    final events = byId.values.toList();
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
        Card(
          child: ListTile(
            leading: StatusDot(online: ts.online),
            title: Text(ts.id),
            subtitle: Text('${ts.vendor} · ${ts.model} · ${ts.protocol}'),
            trailing: Text(onlineLabel(context, ts.online)),
          ),
        ),
        for (final p in pts)
          ListTile(
            title: Text('${p.pointId} · ${p.name}'),
            subtitle: Text('${p.type} · ${p.state}'),
          ),
        if (_status != null)
          Padding(
            padding: const EdgeInsets.only(top: 4, bottom: 8),
            child: Text(
              l10n.accessStatusLine(_status!.state, _status!.users),
              key: const Key('access-status'),
            ),
          ),
        if (_isAdmin && _users.isNotEmpty)
          Text(
            _users.map((u) => u.uid).join(' · '),
            key: const Key('access-uid-list'),
          ),
        if (!_canOpen)
          Padding(
            padding: const EdgeInsets.only(bottom: 8, top: 8),
            child: Text(
              l10n.accessViewerDenied,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        if (_canOpen && !_isAdmin)
          Padding(
            padding: const EdgeInsets.only(bottom: 8),
            child: Text(l10n.accessOperatorHint),
          ),
        const SizedBox(height: 8),
        FilledButton.tonal(
          key: const Key('access-open'),
          onPressed: _canOpen && !_busy ? _confirmAndOpen : null,
          child: Text(l10n.accessOpen),
        ),
        const SizedBox(height: 8),
        FilledButton.tonal(
          key: const Key('access-lock'),
          onPressed: _canOpen && !_busy ? _confirmAndLock : null,
          child: Text(l10n.accessLock),
        ),
        const SizedBox(height: 8),
        Text(
          l10n.accessHint(ts.capabilities.join(', ')),
          style: Theme.of(context).textTheme.bodySmall,
        ),
        if (_last != null) ...[
          const SizedBox(height: 16),
          Text(l10n.controlLastResult, style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 8),
          DualAckChip(
            key: const Key('access-ack'),
            meshOk: _last!.meshOk,
            deviceOk: _last!.deviceOk,
            timeout: _last!.timedOut,
            showTimeout: true,
          ),
          if (_last!.error != null)
            Padding(
              padding: const EdgeInsets.only(top: 8),
              child: Text(
                _last!.error == 'BUSY' ? l10n.accessBusy : 'error: ${_last!.error}',
                key: const Key('access-error'),
              ),
            ),
        ],
        const SizedBox(height: 16),
        Row(
          children: [
            Text(l10n.accessEmployees, style: Theme.of(context).textTheme.titleSmall),
            const Spacer(),
            if (_isAdmin)
              FilledButton.icon(
                key: const Key('access-add'),
                onPressed: _busy ? null : () => _editUser(null),
                icon: const Icon(Icons.person_add_outlined),
                label: Text(l10n.accessAddEmployee),
              ),
          ],
        ),
        if (!_isAdmin)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(l10n.accessAdminListHint),
          ),
        if (_isAdmin && _users.isEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(l10n.accessEmptyEmployees),
          ),
        if (_isAdmin)
          for (final u in _users)
            ListTile(
              key: Key('access-user-${u.uid}'),
              leading: accessPhotoAvatar(widget.session.photos.pathFor(u.photoKey)),
              title: Text(u.displayName),
              subtitle: Text(
                '${u.uid}'
                '${u.employeeId != null && u.employeeId!.isNotEmpty ? ' · ${u.employeeId}' : ''}'
                '${u.enabled ? '' : ' · ${l10n.accessDisabled}'}',
              ),
              trailing: IconButton(
                key: Key('access-edit-${u.uid}'),
                icon: const Icon(Icons.edit_outlined),
                onPressed: _busy ? null : () => _editUser(u),
              ),
              onTap: _busy ? null : () => _editUser(u),
            ),
        const SizedBox(height: 16),
        Text(l10n.accessEvents, style: Theme.of(context).textTheme.titleSmall),
        if (events.isEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(l10n.eventsEmpty),
          )
        else
          for (final e in events)
            ListTile(
              key: Key('access-event-${e.id}'),
              title: Text('${e.type} · ${e.status ?? e.state ?? ''}'),
              subtitle: Text(
                '${e.id} · uid ${e.rfidUid ?? '—'} · source ${e.source}'
                ' · mesh_ok ${e.meshOk} · device_ok ${e.deviceOk}',
              ),
              isThreeLine: true,
            ),
        if (_note != null && _last?.error == null)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(_note!),
          ),
        if (_busy)
          const Padding(
            padding: EdgeInsets.only(top: 16),
            child: Center(child: CircularProgressIndicator()),
          ),
        ],
      ),
    );
  }
}
