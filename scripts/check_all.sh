#!/bin/sh
# Baut alles und fuehrt alle Tests aus; bricht beim ersten Fehler ab.
# Vor jedem Commit laufen lassen:  sh scripts/check_all.sh
set -eu
cd "$(dirname "$0")/.."

echo "== Unit- und Szenario-Tests"
pio test -e native
echo "== dieselben Tests mit Speicherfehler-Erkennung"
pio test -e native_sanitize
echo "== Simulator bauen"
pio run -e simulator
echo "== Screenshot-Tests"
python3 tools/screenshot_test.py
echo "== Web-Oberflaeche"
node --test web/test/*.test.js
echo "== Hilfsprogramme"
python3 -m unittest discover -s tools
echo "== Firmware bauen"
pio run -e device

echo
echo "Alles in Ordnung."
