# Golf-Tracker

Golf-App für das Waveshare ESP32-S3-Touch-AMOLED-1.75 (rundes 466 × 466 AMOLED,
GPS, zwei Knöpfe): Distanz zum Grün, Schläge, Schlägerwahl und Scorecard.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `lib/core` | Kern ohne Hardware und ohne LVGL: Berechnungen, Runde, Logik. Vollständig auf dem PC testbar. |
| `lib/ui` | Oberfläche mit LVGL. Läuft unverändert auf dem Gerät und im Simulator. |
| `src/device` | Einstieg der Firmware für das Gerät. |
| `src/simulator` | Einstieg des PC-Simulators (SDL2-Fenster). |
| `test` | Unit-Tests für den Kern. |

## Befehle

Voraussetzung: [PlatformIO](https://platformio.org/install/cli) und SDL2
(Linux: `sudo apt install libsdl2-dev`, macOS: `brew install sdl2 pkg-config`).

```sh
pio test -e native                       # Unit-Tests auf dem PC
pio test -e native_sanitize              # dieselben Tests mit Speicherfehler-Erkennung
pio run -e simulator                     # Simulator bauen
.pio/build/simulator/program             # Simulator starten
pio run -e device                        # Firmware bauen
pio run -e device -t upload              # Firmware aufs Gerät laden
```

Bei jedem Push laufen Tests, Simulator-Build und Firmware-Build automatisch
über GitHub Actions.

## Stand

Schritt 2 von 13: Distanzberechnung und GPS-Daten lesen sind fertig. Der Display-Treiber für das Gerät fehlt noch;
die Firmware zeichnet vorerst in einen Platzhalter-Puffer.
