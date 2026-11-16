import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:mesh485/app.dart';
import 'package:mesh485/client/fixture_client.dart';
import 'package:mesh485/l10n/app_localizations.dart';
import 'package:mesh485/screens/shell.dart';

Widget app({ConsolePage page = ConsolePage.dashboard, String role = 'viewer'}) {
  return Mesh485App(
    client: FixtureMesh485Client(),
    skipLogin: true,
    role: role,
    initialPage: page,
  );
}

void main() {
  testWidgets('login stub continues as viewer', (tester) async {
    await tester.pumpWidget(
      Mesh485App(client: FixtureMesh485Client()),
    );
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.text(l10n.roleViewer), findsWidgets);
    await tester.tap(find.byKey(const Key('login-continue')));
    await tester.pumpAndSettle();
    expect(find.text(l10n.navDashboard), findsWidgets);
    expect(find.textContaining('EXT-001'), findsOneWidget);
  });

  testWidgets('devices fixture ids match firmware registry', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.devices));
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.text('RAW-001'), findsOneWidget);
    expect(find.text('TS-001'), findsOneWidget);
    expect(find.text('PP-001'), findsOneWidget);
    expect(find.text('S10-001'), findsOneWidget);
    expect(find.text('S20-001'), findsOneWidget);
    expect(find.text('PCN-001'), findsOneWidget);
    expect(find.textContaining(l10n.offline), findsWidgets);
  });

  testWidgets('objects fixture includes S20-001', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.objects));
    await tester.pumpAndSettle();
    expect(find.text('S20-001'), findsWidgets);
    expect(find.textContaining('S20-001'), findsWidgets);
  });

  testWidgets('events show dual ACK flags', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.events));
    await tester.pumpAndSettle();
    expect(find.textContaining('mesh_ok'), findsWidgets);
    expect(find.textContaining('device_ok'), findsWidgets);
    expect(find.textContaining('TS-001'), findsOneWidget);
    expect(find.text('CARD · TS-001'), findsOneWidget);
    expect(find.text('PCN · PCN-001'), findsOneWidget);
    expect(find.textContaining('state ALARM'), findsOneWidget);
  });

  testWidgets('mesh is extender health not chat', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.mesh));
    await tester.pumpAndSettle();
    expect(find.textContaining('RSSI'), findsOneWidget);
    expect(find.textContaining('SNR'), findsOneWidget);
    expect(find.text('Contacts'), findsNothing);
    expect(find.text('Chat'), findsNothing);
    expect(find.text('Channels'), findsNothing);
  });

  testWidgets('bridge is READ_ONLY placeholder', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.bridge));
    await tester.pumpAndSettle();
    expect(find.text('READ_ONLY'), findsOneWidget);
    expect(find.text('off'), findsOneWidget);
  });

  testWidgets('access OPEN disabled for viewer', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.access));
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.text('TS-001'), findsOneWidget);
    final open = tester.widget<FilledButton>(
      find.byKey(const Key('access-open')),
    );
    expect(open.onPressed, isNull);
    expect(find.text(l10n.accessViewerDenied), findsOneWidget);
  });

  testWidgets('operator OPEN confirms then fixture TIMEOUT', (tester) async {
    await tester.pumpWidget(
      Mesh485App(
        client: FixtureMesh485Client(),
        skipLogin: true,
        role: 'operator',
        initialPage: ConsolePage.access,
      ),
    );
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    final open = tester.widget<FilledButton>(
      find.byKey(const Key('access-open')),
    );
    expect(open.onPressed, isNotNull);
    await tester.tap(find.byKey(const Key('access-open')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('access-confirm')), findsOneWidget);
    await tester.tap(find.byKey(const Key('access-execute')));
    await tester.pumpAndSettle();
    expect(find.textContaining(l10n.ackMesh), findsWidgets);
    expect(find.textContaining(l10n.ackDevice), findsWidgets);
    expect(find.textContaining(l10n.ackTimeout), findsWidgets);
    expect(find.textContaining('TIMEOUT'), findsWidgets);
  });

  testWidgets('nav destinations exist', (tester) async {
    await tester.pumpWidget(app());
    await tester.pumpAndSettle();
    for (final name in [
      'dashboard',
      'objects',
      'devices',
      'events',
      'control',
      'mesh',
      'bridge',
      'access',
      'settings',
    ]) {
      expect(find.byKey(Key('nav-$name')), findsOneWidget);
    }
  });

  testWidgets('control confirms then shows dual ACK timeout', (tester) async {
    await tester.pumpWidget(
      Mesh485App(
        client: FixtureMesh485Client(),
        skipLogin: true,
        role: 'operator',
        initialPage: ConsolePage.control,
      ),
    );
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.textContaining('CONTROL_RELAY'), findsWidgets);
    expect(find.textContaining('R1'), findsWidgets);
    expect(find.textContaining('DISABLE_FIRE_ALARM'), findsNothing);
    await tester.tap(find.byKey(const Key('control-on-PP-001-R1')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('control-confirm')), findsOneWidget);
    await tester.tap(find.byKey(const Key('control-execute')));
    await tester.pumpAndSettle();
    expect(find.textContaining(l10n.ackMesh), findsWidgets);
    expect(find.textContaining(l10n.ackDevice), findsWidgets);
    expect(find.textContaining(l10n.ackTimeout), findsWidgets);
    expect(find.textContaining('TIMEOUT'), findsWidgets);
  });

  testWidgets('viewer cannot press CONTROL buttons', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.control));
    await tester.pumpAndSettle();
    final on = tester.widget<FilledButton>(
      find.byKey(const Key('control-on-PP-001-R1')),
    );
    expect(on.onPressed, isNull);
  });

  testWidgets('login can pick operator', (tester) async {
    await tester.pumpWidget(
      Mesh485App(client: FixtureMesh485Client()),
    );
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.text(l10n.roleOperator), findsWidgets);
    expect(find.text(l10n.roleAdmin), findsWidgets);
  });

  testWidgets('viewer cannot OPEN LOCK or mutate cards', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.access));
    await tester.pumpAndSettle();
    final open = tester.widget<FilledButton>(
      find.byKey(const Key('access-open')),
    );
    final lock = tester.widget<FilledButton>(
      find.byKey(const Key('access-lock')),
    );
    expect(open.onPressed, isNull);
    expect(lock.onPressed, isNull);
    expect(find.byKey(const Key('access-add')), findsNothing);
    expect(
      find.byKey(const Key('access-event-00000000-0000-4000-8000-000000000001')),
      findsOneWidget,
    );
  });

  testWidgets('operator LOCK confirms then fixture TIMEOUT', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.access, role: 'operator'));
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    final lock = tester.widget<FilledButton>(
      find.byKey(const Key('access-lock')),
    );
    expect(lock.onPressed, isNotNull);
    expect(find.byKey(const Key('access-add')), findsNothing);
    await tester.tap(find.byKey(const Key('access-lock')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('access-lock-confirm')), findsOneWidget);
    await tester.tap(find.byKey(const Key('access-lock-execute')));
    await tester.pumpAndSettle();
    expect(find.textContaining(l10n.ackTimeout), findsWidgets);
    expect(find.textContaining('TIMEOUT'), findsWidgets);
  });

  testWidgets('admin adds named employee and UID in fixture', (tester) async {
    await tester.pumpWidget(app(page: ConsolePage.access, role: 'admin'));
    await tester.pumpAndSettle();
    final l10n = lookupAppLocalizations(const Locale('ru'));
    expect(find.byKey(const Key('access-add')), findsOneWidget);
    await tester.tap(find.byKey(const Key('access-add')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('access-employee-dialog')), findsOneWidget);
    await tester.enterText(find.byKey(const Key('access-employee-name')), 'Ivan');
    await tester.enterText(find.byKey(const Key('access-employee-id')), 'E18');
    await tester.enterText(find.byKey(const Key('access-uid')), 'cafe babe');
    await tester.enterText(
      find.byKey(const Key('access-photo')),
      r'D:\photos\ivan.png',
    );
    await tester.tap(find.byKey(const Key('access-save')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('access-uid-list')), findsOneWidget);
    expect(find.textContaining('CAFEBABE'), findsWidgets);
    expect(find.byKey(const Key('access-user-CAFEBABE')), findsOneWidget);
    expect(find.text('Ivan'), findsOneWidget);
    expect(find.textContaining('CAFEBABE'), findsWidgets);
    expect(find.textContaining('E18'), findsOneWidget);
    expect(find.text(l10n.accessPhotoHint), findsNothing);
  });
}
