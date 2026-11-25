# Parlo – App zum Golf-Tracker

Flutter-App für Android und iOS. Sie holt die Runden vom Gerät, wertet sie aus
und versorgt das Gerät mit Plätzen und Einstellungen. Plan und Stand:
[docs/flutter-app-plan.md](../docs/flutter-app-plan.md).

```
flutter pub get      # lädt Pakete, erzeugt die Übersetzungen
flutter analyze
flutter test
flutter run          # mit angeschlossenem Handy oder Emulator
```

Aufbau von `lib/`:

| Datei | Inhalt |
|---|---|
| `main.dart`, `app.dart` | Start, Aussehen, Sprache |
| `router.dart`, `shell.dart` | die vier Bereiche und die untere Leiste |
| `pages/` | Ansichten |
| `l10n/app_de.arb` | alle Texte |
| `app_info.dart` | Version der App und der Geräte-Schnittstelle |
