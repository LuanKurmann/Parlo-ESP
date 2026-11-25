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
| `model/` | Platz, Runde, Schläger, Einstellungen im Format des Geräts |
| `data/` | Datenbank (Drift), Speicherzugriff, Sicherung |
| `app_info.dart` | Version der App und der Geräte-Schnittstelle |

## Datenformat

Runden, Plätze und Einstellungen haben in der App dasselbe JSON-Format wie auf
dem Gerät. Die Dateien in `../testdata/format` müssen Firmware und App gleich
lesen und schreiben; beide Testreihen prüfen das.

## Datenbank ändern

1. Tabellen in `lib/data/database.dart` ändern, `schemaVersion` erhöhen,
   Migration in `onUpgrade` ergänzen.
2. `dart run build_runner build` und
   `dart run drift_dev schema dump lib/data/database.dart drift_schemas/`.
3. Migration testen und die erzeugten Dateien mit einchecken.

`sh ../scripts/check_app.sh` prüft alles, auch ob die erzeugten Dateien zum
Code passen.
