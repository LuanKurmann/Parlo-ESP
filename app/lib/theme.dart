import 'package:material_ui/material_ui.dart';

/// Farben wie auf dem Geraet: schwarzer Grund, gruener Akzent.
abstract final class ParloColors {
  static const Color background = Color(0xFF000000);
  static const Color surface = Color(0xFF101410);
  static const Color accent = Color(0xFF3DDC84);
  static const Color text = Color(0xFFF2F5F2);
  static const Color textDim = Color(0xFF8A938A);
}

ThemeData buildParloTheme() {
  final scheme =
      ColorScheme.fromSeed(
        seedColor: ParloColors.accent,
        brightness: Brightness.dark,
      ).copyWith(
        primary: ParloColors.accent,
        onPrimary: Colors.black,
        surface: ParloColors.background,
        onSurface: ParloColors.text,
        onSurfaceVariant: ParloColors.textDim,
        surfaceContainer: ParloColors.surface,
      );
  return ThemeData(
    colorScheme: scheme,
    scaffoldBackgroundColor: ParloColors.background,
    appBarTheme: const AppBarTheme(
      backgroundColor: ParloColors.background,
      foregroundColor: ParloColors.text,
      centerTitle: false,
    ),
    navigationBarTheme: NavigationBarThemeData(
      backgroundColor: ParloColors.surface,
      indicatorColor: ParloColors.accent.withValues(alpha: 0.22),
    ),
  );
}
