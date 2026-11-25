# Golf-Tracker

Golf-App für das Waveshare ESP32-S3-Touch-AMOLED-1.75 (rundes 466 × 466 AMOLED,
GPS, zwei Knöpfe): Distanz zum Grün, Schläge, Schlägerwahl und Scorecard.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `lib/core` | Kern ohne Hardware und ohne LVGL: Berechnungen, Runde, Logik. Vollständig auf dem PC testbar. |
| `lib/ui` | Oberfläche mit LVGL. Läuft unverändert auf dem Gerät und im Simulator. |
| `src/device` | Firmware für das Gerät: Treiber und Verdrahtung mit dem Kern. |
| `docs` | Stand, Inbetriebnahme, Plan der App, Bluetooth-Protokoll. |
| `src/simulator` | Einstieg des PC-Simulators (SDL2-Fenster). |
| `test` | Unit- und Szenario-Tests für den Kern. |
| `tools` | Hilfsprogramme für den PC (GPX-Umwandler, Screenshot-Tests, Referenz-Client für das Bluetooth-Protokoll). |
| `web` | Web-Oberfläche fürs Handy: Runden auswerten, Platz laden, Schlagweiten einstellen. |

## Befehle

Voraussetzung: [PlatformIO](https://platformio.org/install/cli) und SDL2
(Linux: `sudo apt install libsdl2-dev`, macOS: `brew install sdl2 pkg-config`).

```sh
pio test -e native                       # Unit-Tests auf dem PC
pio test -e native_sanitize              # dieselben Tests mit Speicherfehler-Erkennung
pio run -e simulator                     # Simulator bauen
python3 tools/screenshot_test.py         # jede Ansicht mit ihrem Referenzbild vergleichen
node --test web/test/*.test.js           # Web-Oberfläche testen (braucht Node 22 und Playwright)
node --test tools/link/*.test.js         # Bluetooth-Protokoll gegen den Simulator testen
.pio/build/simulator/program             # Simulator starten (Tasten siehe unten)
pio run -e device                        # Firmware bauen
pio run -e device -t upload              # Firmware aufs Gerät laden
sh scripts/check_all.sh                  # alles bauen und alle Tests ausführen
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

### GPS-Aufzeichnung abspielen

Statt mit den Pfeiltasten kann der Simulator eine aufgezeichnete Strecke
abspielen. Die Daten laufen dabei durch denselben Weg wie auf dem Gerät.

```sh
.pio/build/simulator/program --demo-track demo.nmea        # Beispielaufzeichnung erzeugen (3 Löcher)
.pio/build/simulator/program --replay demo.nmea --speed 10 # im Zeitraffer abspielen
```

Eine eigene Aufzeichnung vom Handy (GPX) lässt sich umwandeln:

```sh
python3 tools/gpx_to_nmea.py runde.gpx runde.nmea
```

Ohne Fenster spielt `play:60` die nächsten 60 Sekunden der Aufzeichnung ab.

## Screenshot-Tests

`tools/screenshot_test.py` rendert 27 Zustände der Oberfläche im Simulator und
vergleicht sie mit den Referenzbildern in `test/screens/golden/`. Zusätzlich
prüft es, dass nichts über den runden Displayrand hinaus gezeichnet wird.
Welche Zustände das sind, steht in `test/screens/screens.json`.

Nach einer gewollten Änderung am Aussehen werden die Referenzbilder neu erzeugt
und mit eingecheckt (vorher anschauen):

```sh
python3 tools/screenshot_test.py --update
```

Schlägt der Test fehl, liegen das aktuelle Bild und ein Differenzbild in
`screenshot_failures/`. Voraussetzung ist Pillow (`pip install pillow`).

## Verbindung zum Handy

Das Gerät stellt eine Web-Oberfläche bereit, die im Handy-Browser läuft:

- **Runden:** laufende Runde, Archiv, Scorecard, Schläge mit Weiten,
  Durchschnitt pro Schläger, Export als CSV oder JSON.
- **Platz:** Platz nach Namen in OpenStreetMap suchen, Vorschau prüfen und aufs
  Gerät laden; Platzdatei sichern und zurückspielen.
- **Schläger:** das eigene Bag zusammenstellen (bis 14 Schläger) und
  Schlagweiten für den Schlägervorschlag eintragen.
- **Gerät:** Helligkeit, Zeit bis zum Dimmen, Knöpfe tauschen, WLAN eintragen.

Im Simulator ausprobieren:

```sh
.pio/build/simulator/program --serve 8080     # dann http://127.0.0.1:8080/ öffnen
```

Die Schnittstelle dahinter (`/api/...`) ist in `lib/core/src/core/api.h`
beschrieben. Sie ist nicht passwortgeschützt und nur für das eigene WLAN gedacht.

Auf dem Gerät startet das WLAN über Menü → „Mit Handy verbinden“. Ohne
gespeichertes WLAN spannt das Gerät einen eigenen, passwortgeschützten Hotspot
auf; Name, Passwort und Adresse stehen dann auf dem Display. Im Bereich „Gerät“
der Web-Oberfläche lässt sich das eigene WLAN eintragen.

## Stand

Schritte 1 bis 11 und der hardwareunabhängige Teil von Schritt 13 sind fertig:
Kern, Oberfläche, Handy-Verbindung, Akkuanzeige und Display-Stromsparen sind
getestet. Die Gerätetreiber (Display, Touch, Knöpfe, GPS, Speicher, WLAN, Akku)
sind eingebunden und kompilieren, sind aber noch nie auf dem Board gelaufen.
Offen ist Schritt 12, die Inbetriebnahme am Gerät: siehe
[docs/inbetriebnahme.md](docs/inbetriebnahme.md).
