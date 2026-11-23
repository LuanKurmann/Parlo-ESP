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
.pio/build/simulator/program             # Simulator starten (Tasten siehe unten)
pio run -e device                        # Firmware bauen
pio run -e device -t upload              # Firmware aufs Gerät laden
```

Bei jedem Push laufen Tests, Simulator-Build und Firmware-Build automatisch
über GitHub Actions.

## Simulator

Der Simulator zeigt die Oberfläche des Geräts in einem Fenster. Platz und
Runde liegen im Ordner `sim_data` und bleiben zwischen den Starts erhalten.

| Taste | Wirkung |
|---|---|
| `1`, `2` | Knopf 1 und Knopf 2, kurz tippen oder lang halten |
| Maus | Touch: tippen und wischen |
| Pfeil links / rechts | Ansicht wechseln (wie Wischen) |
| Pfeil hoch / ab | auf der Bahn Richtung Grün bzw. zurück gehen |
| `G` | GPS-Empfang aus- und einschalten |
| `S` | Screenshot nach `screenshot.bmp` |

Ohne Fenster lässt er sich mit Befehlen steuern, zum Beispiel für Screenshots:

```sh
.pio/build/simulator/program --headless --do "b1,b1,pos:218,shot:distanz.bmp"
```

`b1`/`b2` drücken einen Knopf kurz, `B1`/`B2` lang, `pos:218` stellt den
Spieler 218 m hinter den Abschlag, `gps:off`/`gps:on` schaltet den Empfang.
`click:233:300` tippt das Display an, `swipe:left`/`swipe:right` wischt,
`wait:2000` wartet zwei Sekunden.

## Stand

Schritt 7 von 13: Der Kern ist fertig und speichert sicher gegen Stromausfall.
Der Simulator zeigt alle Ansichten (Distanz, Schläge, Scorecard) samt
Schlägerwahl, Menü, Loch- und Rundenübersicht, bedienbar über Knöpfe und Touch.
Der Display-Treiber für das Gerät fehlt noch; die Firmware zeichnet vorerst
in einen Platzhalter-Puffer.
