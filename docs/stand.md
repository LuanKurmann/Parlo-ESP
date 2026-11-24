# Stand des Projekts

Festgehalten am 24. November 2026, Branch `ccr-155b4e86-xh4thc`.

## Kurzfassung

Die Golf-App für das Waveshare ESP32-S3-Touch-AMOLED-1.75 ist in Software
fertig und getestet. Auf dem echten Board ist sie noch nie gelaufen; das ist
der einzige offene Schritt des ursprünglichen Plans.

| Schritt | Inhalt | Stand |
|---|---|---|
| 1 | Projektgerüst, drei Ziele, GitHub Actions | fertig |
| 2 | Distanzberechnung, GPS-Sätze lesen | fertig |
| 3 | Platz, Schläger, Runde, Score | fertig |
| 4 | Knopf-Logik, Ablaufsteuerung | fertig |
| 5 | Speichern, sicher gegen Stromausfall | fertig |
| 6 | Simulator mit Distanz-Ansicht | fertig |
| 7 | Alle Ansichten, Touch | fertig |
| 8 | GPS-Wiedergabe, Szenario-Tests | fertig |
| 9 | Screenshot-Tests | fertig |
| 10 | Handy-Verbindung: Schnittstelle und Web-Oberfläche | fertig |
| 11 | Gerätetreiber | kompiliert, am Gerät ungeprüft |
| 12 | Inbetriebnahme am Board | offen, braucht das Board |
| 13 | Akkuanzeige, Display-Stromsparen | fertig; Laufzeit und weiteres Stromsparen offen |

## Was das Gerät kann

- **Distanz:** vorne, Mitte und hinten des Grüns, dazu Weite des letzten Schlags.
- **Schläge:** Knopf 1 zählt einen Schlag und speichert die Position; danach
  Schlägerwahl mit Vorschlag nach Restdistanz. Putts sind Schläge mit dem Putter.
- **Loch und Runde:** Knopf 2 schliesst das Loch ab; Scorecard, Rundenübersicht,
  Archiv beendeter Runden.
- **Menü:** Grün hier markieren, vorheriges Loch, Runde beenden, mit Handy verbinden.
- **Robustheit:** Runde übersteht Neustart und Stromausfall; GPS-Ausfälle und
  gestörte Daten werden erkannt.
- **Handy:** Web-Oberfläche über WLAN für Auswertung, Platz-Import aus
  OpenStreetMap, Schlagweiten und WLAN-Einrichtung.

### Knopfbelegung

| | Kurz | Lang |
|---|---|---|
| Knopf 1 (BOOT) | Schlag zählen / bestätigen | letzten Schlag löschen |
| Knopf 2 (PWR) | Loch abschliessen / weiterschalten | Menü |

## Aufbau

```
lib/core     Kern ohne Hardware: Logik, Speicherformat, Schnittstelle (C++)
lib/ui       Oberfläche mit LVGL, identisch auf Gerät und Simulator
src/device   Treiber und Verdrahtung für das Board
src/simulator  PC-Simulator: Fenster, Skriptbefehle, Webserver, GPS-Wiedergabe
web          Web-Oberfläche fürs Handy (HTML, CSS, JavaScript ohne Framework)
test         Unit- und Szenario-Tests des Kerns, Referenzbilder
tools        GPX-Umwandler, Screenshot-Tests
docs         dieses Dokument, Inbetriebnahme, Plan der Flutter-App
```

## Tests

`sh scripts/check_all.sh` baut alles und führt alle Tests aus. GitHub Actions
macht dasselbe bei jedem Push.

| Art | Umfang |
|---|---|
| Kern (Unit und Szenario), auch mit Speicherfehler-Erkennung | 387 Tests |
| Web-Oberfläche: Rechenlogik und Browser gegen den Simulator | 37 Tests |
| Screenshot-Tests der Geräte-Oberfläche | 35 Bilder |
| Hilfsprogramme | 17 Tests |

Besonders abgesichert sind: Stromausfall an jeder Stelle des Speicherns, eine
ganze 18-Loch-Runde vom GPS-Zeichenstrom bis zum Archiv, gestörte GPS-Daten
und zufällige Bedienung.

## Wichtige Entscheide

- **Tragweise und Gehäuse:** noch offen; Schrift ist für Trolley und Handgelenk gross genug.
- **Platzdaten:** Import aus OpenStreetMap und selbst markieren, beides möglich.
- **Grün:** als Kreis um die Mitte; der Radius kommt aus der OpenStreetMap-Fläche, sonst 13 m.
- **Schlägervorschlag:** der Schläger aus dem Bag mit der nächstliegenden Schlagweite.
- **Bag:** bis zu 14 Schläger aus einem Katalog von 17, einstellbar über die Web-Oberfläche.
- **Kennungen:** Runden, Plätze und das Gerät tragen dauerhafte Kennungen für den Abgleich mit einer App.
- **Neue Runde:** nur mit langem Druck, damit keine Runde versehentlich endet.
- **Display:** dimmt nach 20 Sekunden, geht erst nach 30 Minuten aus, damit
  während des Spiels kein Schlag nur das Display weckt.
- **Koordinaten in Dateien:** Ganzzahlen in Zehnmillionstel Grad, verlustfrei.
- **Speicher:** interner Flash (LittleFS), nicht die SD-Karte.
- **GPS:** über I2C wie im Herstellerbeispiel, nicht seriell.
- **Hotspot:** mit gerätespezifischem Passwort, das nur auf dem Display steht.

## Offen und unsicher

- **Inbetriebnahme (Schritt 12):** Ablauf und Annahmen in
  [inbetriebnahme.md](inbetriebnahme.md). Grösste Unbekannte ist das GPS-Modul.
- **OpenStreetMap:** Abfragen und Umwandlung sind nie gegen den echten Dienst
  gelaufen, nur gegen feste Testantworten.
- **Simulator mit Fenster:** nur ohne Fenster geprüft.
- **Akkulaufzeit:** nicht gemessen; Prozessor und GPS laufen durch.
- **Web-Oberfläche:** ohne Passwort, nur fürs eigene WLAN gedacht.
- **Uhrzeit:** Rundenbeginn ist UTC; die Web-Oberfläche rechnet in Ortszeit um.

## Nächste Schritte

1. Board in Betrieb nehmen (Schritt 12).
2. Flutter-App mit Bluetooth-Abgleich: siehe [flutter-app-plan.md](flutter-app-plan.md).
