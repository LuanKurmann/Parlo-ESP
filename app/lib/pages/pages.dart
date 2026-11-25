import 'package:material_ui/material_ui.dart';

import '../app_info.dart';
import '../l10n/app_localizations.dart';
import 'placeholder_page.dart';

class RoundsPage extends StatelessWidget {
  const RoundsPage({super.key});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return PlaceholderPage(
      title: l.tabRounds,
      icon: Icons.flag_outlined,
      headline: l.roundsEmptyTitle,
      text: l.roundsEmptyText,
    );
  }
}

class CoursesPage extends StatelessWidget {
  const CoursesPage({super.key});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return PlaceholderPage(
      title: l.tabCourses,
      icon: Icons.map_outlined,
      headline: l.coursesEmptyTitle,
      text: l.coursesEmptyText,
    );
  }
}

class DevicePage extends StatelessWidget {
  const DevicePage({super.key});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return PlaceholderPage(
      title: l.tabDevice,
      icon: Icons.watch_outlined,
      headline: l.deviceEmptyTitle,
      text: l.deviceEmptyText,
    );
  }
}

class SettingsPage extends StatelessWidget {
  const SettingsPage({super.key});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return PlaceholderPage(
      title: l.tabSettings,
      icon: Icons.tune,
      headline: l.settingsEmptyTitle,
      text: l.settingsEmptyText,
      footer: l.aboutVersion(appVersion),
    );
  }
}
