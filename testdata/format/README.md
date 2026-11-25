# Gemeinsame Testdaten für das Dateiformat

Diese Dateien lesen sowohl die Firmware-Tests (`test/test_format_files`) als
auch die Tests der App (`app/test/model/format_files_test.dart`).

- `course.json`, `round.json`, `settings.json`, `settings_default.json`:
  Beide Seiten müssen die Datei lesen und Byte für Byte gleich wieder schreiben.
- `rejects.json`: Eingaben, die beide Seiten ablehnen müssen.
- `accepts.json`: Eingaben, die beide annehmen, und was sie daraus schreiben.

Wer das Format ändert, ändert es damit sichtbar für beide Seiten.
