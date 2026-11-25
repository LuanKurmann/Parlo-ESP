import 'package:go_router/go_router.dart';

import 'pages/pages.dart';
import 'shell.dart';

abstract final class Routes {
  static const rounds = '/runden';
  static const courses = '/plaetze';
  static const device = '/geraet';
  static const settings = '/einstellungen';
}

GoRouter buildRouter({String initialLocation = Routes.rounds}) {
  return GoRouter(
    initialLocation: initialLocation,
    routes: [
      StatefulShellRoute.indexedStack(
        builder: (context, state, shell) => AppShell(shell: shell),
        branches: [
          StatefulShellBranch(
            routes: [
              GoRoute(
                path: Routes.rounds,
                builder: (context, state) => const RoundsPage(),
              ),
            ],
          ),
          StatefulShellBranch(
            routes: [
              GoRoute(
                path: Routes.courses,
                builder: (context, state) => const CoursesPage(),
              ),
            ],
          ),
          StatefulShellBranch(
            routes: [
              GoRoute(
                path: Routes.device,
                builder: (context, state) => const DevicePage(),
              ),
            ],
          ),
          StatefulShellBranch(
            routes: [
              GoRoute(
                path: Routes.settings,
                builder: (context, state) => const SettingsPage(),
              ),
            ],
          ),
        ],
      ),
    ],
  );
}
