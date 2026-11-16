import 'dart:async';

import 'package:flutter/material.dart';

import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../widgets/chrome.dart';
import 'access_screen.dart';
import 'bridge_screen.dart';
import 'control_screen.dart';
import 'dashboard_screen.dart';
import 'devices_screen.dart';
import 'events_screen.dart';
import 'mesh_screen.dart';
import 'objects_screen.dart';
import 'settings_screen.dart';

enum ConsolePage {
  dashboard,
  objects,
  devices,
  events,
  control,
  mesh,
  bridge,
  access,
  settings,
}

/// Shared chrome for Windows and Android. Destinations match `docs/apps.md`.
class ConsoleShell extends StatefulWidget {
  const ConsoleShell({
    super.key,
    required this.session,
    required this.role,
    this.initialPage = ConsolePage.dashboard,
    this.pollInterval,
  });

  final ConsoleSession session;
  final String role;
  final ConsolePage initialPage;
  final Duration? pollInterval;

  @override
  State<ConsoleShell> createState() => _ConsoleShellState();
}

class _ConsoleShellState extends State<ConsoleShell> {
  late ConsolePage _page = widget.initialPage;
  Timer? _poll;

  @override
  void initState() {
    super.initState();
    widget.session.addListener(_onSession);
    final every = widget.pollInterval;
    if (every != null) {
      _poll = Timer.periodic(every, (_) => widget.session.reload());
    }
  }

  @override
  void dispose() {
    _poll?.cancel();
    widget.session.removeListener(_onSession);
    super.dispose();
  }

  void _onSession() {
    if (mounted) setState(() {});
  }

  List<({ConsolePage page, String label, IconData icon, IconData selected})>
      _destinations(AppLocalizations l10n) => [
            (
              page: ConsolePage.dashboard,
              label: l10n.navDashboard,
              icon: Icons.dashboard_outlined,
              selected: Icons.dashboard,
            ),
            (
              page: ConsolePage.objects,
              label: l10n.navObjects,
              icon: Icons.apartment_outlined,
              selected: Icons.apartment,
            ),
            (
              page: ConsolePage.devices,
              label: l10n.navDevices,
              icon: Icons.developer_board_outlined,
              selected: Icons.developer_board,
            ),
            (
              page: ConsolePage.events,
              label: l10n.navEvents,
              icon: Icons.list_alt_outlined,
              selected: Icons.list_alt,
            ),
            (
              page: ConsolePage.control,
              label: l10n.navControl,
              icon: Icons.tune_outlined,
              selected: Icons.tune,
            ),
            (
              page: ConsolePage.mesh,
              label: l10n.navMesh,
              icon: Icons.cell_tower_outlined,
              selected: Icons.cell_tower,
            ),
            (
              page: ConsolePage.bridge,
              label: l10n.navBridge,
              icon: Icons.settings_ethernet_outlined,
              selected: Icons.settings_ethernet,
            ),
            (
              page: ConsolePage.access,
              label: l10n.navAccess,
              icon: Icons.sensor_door_outlined,
              selected: Icons.sensor_door,
            ),
            (
              page: ConsolePage.settings,
              label: l10n.navSettings,
              icon: Icons.settings_outlined,
              selected: Icons.settings,
            ),
          ];

  void _go(ConsolePage page) {
    setState(() => _page = page);
    if (Scaffold.maybeOf(context)?.isDrawerOpen ?? false) {
      Navigator.of(context).pop();
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    final dest = _destinations(l10n);
    final title = dest.firstWhere((d) => d.page == _page).label;
    final data = widget.session.snapshot;
    if (data == null) {
      return Scaffold(
        appBar: AppBar(title: Text(l10n.appTitle)),
        body: Center(
          child: widget.session.lastError == null
              ? const CircularProgressIndicator()
              : Text('${widget.session.lastError}'),
        ),
      );
    }
    final body = _pageBody(data);
    return LayoutBuilder(
      builder: (context, constraints) {
        final wide = constraints.maxWidth >= 720;
        if (wide) {
          return Scaffold(
            appBar: _appBar(title),
            body: Row(
              children: [
                SizedBox(
                  width: 200,
                  child: ListView(
                    children: [
                      for (final d in dest)
                        ListTile(
                          key: Key('nav-${d.page.name}'),
                          leading: Icon(
                            _page == d.page ? d.selected : d.icon,
                          ),
                          title: Text(d.label),
                          selected: _page == d.page,
                          onTap: () => _go(d.page),
                        ),
                    ],
                  ),
                ),
                const VerticalDivider(width: 1),
                Expanded(child: body),
              ],
            ),
          );
        }
        return Scaffold(
          appBar: _appBar(title),
          drawer: NavigationDrawer(
            selectedIndex: _page.index,
            onDestinationSelected: (i) => _go(dest[i].page),
            children: [
              Padding(
                padding: const EdgeInsets.fromLTRB(28, 16, 16, 10),
                child: Text(l10n.appTitle),
              ),
              for (final d in dest)
                NavigationDrawerDestination(
                  icon: Icon(d.icon),
                  selectedIcon: Icon(d.selected),
                  label: Text(d.label),
                ),
            ],
          ),
          body: body,
        );
      },
    );
  }

  PreferredSizeWidget _appBar(String title) {
    return AppBar(
      title: Text(title),
      actions: [
        RoleChip(role: widget.role),
        IconButton(
          tooltip: AppLocalizations.of(context).settingsRefresh,
          onPressed: () => widget.session.reload(),
          icon: const Icon(Icons.refresh),
        ),
        const SizedBox(width: 8),
      ],
    );
  }

  Widget _pageBody(ConsoleSnapshot data) {
    switch (_page) {
      case ConsolePage.dashboard:
        return DashboardScreen(data: data);
      case ConsolePage.objects:
        return ObjectsScreen(objects: data.objects, devices: data.devices);
      case ConsolePage.devices:
        return DevicesScreen(devices: data.devices, points: data.points);
      case ConsolePage.events:
        return EventsScreen(events: data.events);
      case ConsolePage.control:
        return ControlScreen(
          session: widget.session,
          devices: data.devices,
          points: data.points,
          role: widget.role,
        );
      case ConsolePage.mesh:
        return MeshScreen(health: data.health);
      case ConsolePage.bridge:
        return BridgeScreen(bridge: data.bridge);
      case ConsolePage.access:
        return AccessScreen(
          session: widget.session,
          devices: data.devices,
          points: data.points,
          events: data.events,
          role: widget.role,
        );
      case ConsolePage.settings:
        return SettingsScreen(session: widget.session, role: widget.role);
    }
  }
}
