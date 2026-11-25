import 'package:go_router/go_router.dart';
import 'package:material_ui/material_ui.dart';

import 'l10n/app_localizations.dart';
import 'router.dart';
import 'theme.dart';

class ParloApp extends StatefulWidget {
  const ParloApp({super.key, this.initialLocation = Routes.rounds});

  final String initialLocation;

  @override
  State<ParloApp> createState() => _ParloAppState();
}

class _ParloAppState extends State<ParloApp> {
  late final GoRouter _router = buildRouter(
    initialLocation: widget.initialLocation,
  );

  @override
  void dispose() {
    _router.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp.router(
      onGenerateTitle: (context) => AppLocalizations.of(context).appTitle,
      theme: buildParloTheme(),
      routerConfig: _router,
      // Nicht die erzeugte Liste: sie verweist noch auf die abgeloesten
      // Uebersetzungen aus flutter_localizations.
      localizationsDelegates: const [
        AppLocalizations.delegate,
        ...GlobalMaterialLocalizations.delegates,
      ],
      supportedLocales: AppLocalizations.supportedLocales,
      debugShowCheckedModeBanner: false,
    );
  }
}
