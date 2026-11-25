import 'package:go_router/go_router.dart';
import 'package:material_ui/material_ui.dart';

import 'l10n/app_localizations.dart';

/// Rahmen mit der unteren Leiste; jeder Bereich behaelt seinen Zustand.
class AppShell extends StatelessWidget {
  const AppShell({super.key, required this.shell});

  final StatefulNavigationShell shell;

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return Scaffold(
      body: shell,
      bottomNavigationBar: NavigationBar(
        selectedIndex: shell.currentIndex,
        onDestinationSelected: (index) =>
            shell.goBranch(index, initialLocation: index == shell.currentIndex),
        destinations: [
          NavigationDestination(
            icon: const Icon(Icons.flag_outlined),
            selectedIcon: const Icon(Icons.flag),
            label: l.tabRounds,
          ),
          NavigationDestination(
            icon: const Icon(Icons.map_outlined),
            selectedIcon: const Icon(Icons.map),
            label: l.tabCourses,
          ),
          NavigationDestination(
            icon: const Icon(Icons.watch_outlined),
            selectedIcon: const Icon(Icons.watch),
            label: l.tabDevice,
          ),
          NavigationDestination(
            icon: const Icon(Icons.tune),
            label: l.tabSettings,
          ),
        ],
      ),
    );
  }
}
