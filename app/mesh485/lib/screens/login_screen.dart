import 'package:flutter/material.dart';

import '../client/session.dart';
import '../l10n/app_localizations.dart';
import '../widgets/chrome.dart';
import 'shell.dart';

/// Role picker: VIEWER / OPERATOR / ADMIN. Chip values are factory allow-list
/// names (`viewer` / `operator` / `admin`) the live client puts in `operator`.
class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key, required this.session});

  final ConsoleSession session;

  static const String stubRole = 'viewer';

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  String _role = LoginScreen.stubRole;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context);
    return Scaffold(
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 420),
          child: Card(
            margin: const EdgeInsets.all(24),
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text(
                    l10n.appSubtitle,
                    style: Theme.of(context).textTheme.headlineSmall,
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    l10n.loginHint,
                    style: Theme.of(context).textTheme.bodyMedium,
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 24),
                  SegmentedButton<String>(
                    key: const Key('login-role'),
                    segments: [
                      ButtonSegment(
                        value: 'viewer',
                        label: Text(l10n.roleViewer),
                      ),
                      ButtonSegment(
                        value: 'operator',
                        label: Text(l10n.roleOperator),
                      ),
                      ButtonSegment(
                        value: 'admin',
                        label: Text(l10n.roleAdmin),
                      ),
                    ],
                    selected: {_role},
                    onSelectionChanged: (s) => setState(() => _role = s.first),
                  ),
                  const SizedBox(height: 16),
                  Center(child: RoleChip(role: _role)),
                  const SizedBox(height: 24),
                  FilledButton(
                    key: const Key('login-continue'),
                    onPressed: () {
                      widget.session.setRole(_role);
                      Navigator.of(context).pushReplacement(
                        MaterialPageRoute<void>(
                          builder: (_) => ConsoleShell(
                            session: widget.session,
                            role: _role,
                          ),
                        ),
                      );
                    },
                    child: Text(l10n.loginContinue),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
