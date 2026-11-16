import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import 'client/mesh485_client.dart';
import 'client/session.dart';
import 'l10n/app_localizations.dart';
import 'screens/login_screen.dart';
import 'screens/shell.dart';

class Mesh485App extends StatefulWidget {
  const Mesh485App({
    super.key,
    this.client,
    this.skipLogin = false,
    this.role = LoginScreen.stubRole,
    this.initialPage = ConsolePage.dashboard,
    this.pollInterval,
  });

  final Mesh485Client? client;
  final bool skipLogin;
  final String role;
  final ConsolePage initialPage;
  /// Tests pass null so [pumpAndSettle] does not hang on a periodic timer.
  final Duration? pollInterval;

  @override
  State<Mesh485App> createState() => _Mesh485AppState();
}

class _Mesh485AppState extends State<Mesh485App> {
  late final ConsoleSession session = ConsoleSession(forced: widget.client);

  @override
  void initState() {
    super.initState();
    session.setRole(widget.role);
    session.boot();
  }

  @override
  void dispose() {
    session.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      onGenerateTitle: (ctx) => AppLocalizations.of(ctx).appTitle,
      locale: const Locale('ru'),
      supportedLocales: const [Locale('ru')],
      localizationsDelegates: const [
        AppLocalizations.delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
      ],
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF1B5E20),
          brightness: Brightness.light,
        ),
        useMaterial3: true,
      ),
      darkTheme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF1B5E20),
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
      ),
      home: widget.skipLogin
          ? ConsoleShell(
              session: session,
              role: widget.role,
              initialPage: widget.initialPage,
              pollInterval: widget.pollInterval,
            )
          : LoginScreen(session: session),
    );
  }
}
