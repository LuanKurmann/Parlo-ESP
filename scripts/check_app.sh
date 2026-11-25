#!/bin/sh
# Prueft die Flutter-App: erzeugter Code aktuell, Formatierung, Analyse, Tests.
set -eu
cd "$(dirname "$0")/../app"

flutter pub get >/dev/null
echo "-- erzeugter Datenbank-Code und Schema-Abbild aktuell?"
dart run build_runner build >/dev/null
dart run drift_dev schema dump lib/data/database.dart drift_schemas/ >/dev/null
dart format lib/data/database.g.dart >/dev/null
generated="lib/data/database.g.dart drift_schemas"
# muessen eingecheckt (oder dafuer vorgemerkt) sein und dem Code entsprechen
if ! git ls-files --error-unmatch $generated >/dev/null 2>&1 ||
   ! git diff --quiet -- $generated ||
   [ -n "$(git ls-files --others --exclude-standard -- $generated)" ]; then
  git status --short -- $generated
  echo "Erzeugte Dateien weichen ab. Bei geaendertem Schema: schemaVersion"
  echo "erhoehen, Migration und Test ergaenzen, Dateien mit einchecken."
  exit 1
fi
echo "-- Formatierung"
dart format --output=none --set-exit-if-changed lib test
echo "-- Analyse"
flutter analyze --fatal-infos
echo "-- Tests"
flutter test
