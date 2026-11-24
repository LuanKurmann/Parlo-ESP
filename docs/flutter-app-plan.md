# Plan: Flutter-App zum Golf-Tracker

Stand: Planung, noch kein Code. Grundlage sind die Antworten vom
24. November 2026:

| Frage | Entscheid |
|---|---|
| Verbindung | Bluetooth Low Energy |
| Plattformen | Android und iOS |
| Datenhaltung | nur auf dem Handy, Sicherung als Datei |
| Umfang Version 1 | Abgleich und Auswertung, Plätze suchen und laden, Schläge auf der Karte, Einstellungen für das Gerät (Schlägerauswahl usw.) |

## Ziel

Die App ist das Zuhause der Daten. Das Gerät erfasst auf dem Platz, die App
holt die Runden ab, bewahrt sie dauerhaft auf, wertet sie aus und versorgt das
Gerät mit Plätzen und Einstellungen. Der Abgleich läuft über Bluetooth, ohne
dass am Gerät etwas eingestellt werden muss, und das Handy behält dabei Internet.

Die bestehende Web-Oberfläche über WLAN bleibt als Notlösung ohne App
erhalten, bekommt aber keine neuen Funktionen.

## Grundsätze (wie bisher im Projekt)

- **Kleine Schritte**, jeder mit Tests und einem eigenen Commit.
- **Ohne Hardware testbar**, so weit es geht: Die App spricht in den Tests mit
  dem Simulator statt mit dem Gerät.
- **Eine Schnittstelle:** Bluetooth überträgt dieselben Anfragen wie heute das
  WLAN (`/api/...`). Die 35 Tests der Schnittstelle gelten damit weiter.
- **Gleiche Rechenergebnisse überall:** Distanzen, Score und Statistik werden
  in Gerät (C++), Web (JavaScript) und App (Dart) gegen dieselben Testdaten geprüft.

## Architektur

```
Flutter-App
 ├─ Oberfläche            Runden, Plätze, Karte, Einstellungen
 ├─ Lokale Datenbank      Runden, Plätze, Einstellungen (SQLite)
 ├─ Abgleich              was fehlt wo? holen, senden, zusammenführen
 ├─ Geräte-Client         Anfragen wie GET /api/rounds
 └─ Transport             Bluetooth (Gerät) oder TCP (Simulator, nur Tests)
        │
Golf-Tracker
 ├─ Transport             Bluetooth-Dienst (neu) / WLAN (vorhanden)
 └─ Schnittstelle im Kern handleApi (vorhanden, getestet)
```

### Vorgeschlagene Bausteine

| Zweck | Wahl | Warum |
|---|---|---|
| Zustand | Riverpod | verbreitet, gut testbar |
| Datenbank | Drift (SQLite) | typsicher, Migrationen, läuft in Tests ohne Gerät |
| Bluetooth | flutter_blue_plus | Android und iOS, aktiv gepflegt |
| Karte | flutter_map | frei, Anbieter der Kartenbilder wählbar |
| Diagramme | fl_chart | reicht für Verlauf und Verteilungen |
| Navigation | go_router | Standard |
| Sprache | Deutsch, vorbereitet für weitere | wie das Gerät |

Die App liegt im selben Repository im Ordner `app/`, damit Schnittstelle,
Testdaten und App zusammen versioniert sind.

## Bluetooth-Verbindung

### Übertragung

Ein eigener Bluetooth-Dienst mit zwei Kanälen: einer für Anfragen (App
schreibt), einer für Antworten (Gerät meldet). Eine Anfrage ist dasselbe wie
heute über WLAN, nur verpackt:

```
Anfrage   {"id": 7, "m": "GET", "p": "/api/rounds/3"}
Antwort   {"id": 7, "s": 200, "b": { ... }}
```

Weil ein Bluetooth-Paket klein ist (meist 180 bis 500 Bytes), wird jede
Nachricht in nummerierte Stücke zerlegt und auf der anderen Seite wieder
zusammengesetzt. Dieses Zerlegen und Zusammensetzen kommt in den Kern und wird
dort getestet (verlorene, doppelte, vertauschte Stücke, Abbruch mittendrin).

Erwartete Geschwindigkeit: eine Runde (rund 5 kB) in unter einer Sekunde, ein
Platz ähnlich. Das ist eine Schätzung; gemessen wird am Gerät.

### Koppeln und Sicherheit

- Erstes Verbinden über „Mit Handy verbinden“ am Gerät: Das Display zeigt
  einen sechsstelligen Code, den man in der App eingibt.
- Danach ist die Verbindung verschlüsselt und das Handy bekannt. Die
  Schnittstelle antwortet nur gekoppelten Geräten.
- Nach dem Koppeln ist das Gerät dauerhaft per Bluetooth sichtbar (stromsparend,
  langsames Intervall), damit der Abgleich ohne Knopfdruck geht.

### Unterschiede Android und iOS

- iOS erlaubt Abgleich im Hintergrund nur eingeschränkt. Verlässlich ist der
  Abgleich beim Öffnen der App; alles darüber ist ein Bonus.
- iOS-Builds brauchen einen Mac und ein Apple-Entwicklerkonto (kostenpflichtig),
  um die App aufs eigene iPhone zu bringen. In GitHub Actions lässt sich iOS
  ohne Signatur kompilieren; das stellt sicher, dass der Code für iOS baut.
  Ausprobieren auf einem iPhone kann ich nicht.

## Abgleich

### Runden (Gerät → Handy)

1. App fragt die Liste der Runden auf dem Gerät ab.
2. Sie holt jede Runde, die sie noch nicht hat, und speichert sie.
3. Erst wenn eine Runde sicher gespeichert ist, darf sie auf dem Gerät
   gelöscht werden. Einstellung in der App: „Auf dem Gerät behalten“ (Standard:
   die letzten 20 Runden).

Dafür braucht jede Runde eine **dauerhafte Kennung**. Die heutige Nummer reicht
nicht, weil sie nach dem Löschen neu vergeben werden kann. Die Firmware vergibt
künftig beim Archivieren eine zufällige Kennung.

Runden werden auf dem Handy nie durch einen Abgleich überschrieben oder
gelöscht. Korrekturen (Schläger ändern, Schlag löschen) macht man in der App;
sie bleiben auf dem Handy.

### Plätze (Handy ↔ Gerät)

- Die App führt eine **Platz-Bibliothek**. Auf dem Gerät liegt immer genau ein
  Platz, der aktive.
- „Auf das Gerät laden“ ersetzt den aktiven Platz (eine laufende Runde wird
  wie heute archiviert).
- Grüns, die auf dem Platz am Gerät markiert wurden, fliessen beim nächsten
  Abgleich zurück in die Bibliothek. Dafür bekommt jeder Platz eine Kennung
  und einen Änderungszähler; bei Unterschieden fragt die App nach.

### Einstellungen (Handy → Gerät)

Die App ist die Quelle; das Gerät übernimmt beim Abgleich.

## Funktionen der ersten Version

### 1. Abgleich und Auswertung

- Gerät koppeln, Status (Akku, Platz, laufende Runde), manueller und
  automatischer Abgleich.
- Rundenliste, Rundendetail mit Scorecard und Schlägen.
- Verlauf über mehrere Runden: Score, Putts pro Runde, Schlagweiten pro
  Schläger (Durchschnitt, Streuung, Bestweite).
- Sicherung: alle Daten als eine Datei exportieren und wieder einlesen;
  einzelne Runden als CSV.

### 2. Plätze suchen und laden

- Suche in OpenStreetMap nach Name, zusätzlich „Plätze in meiner Nähe“ (in der
  App möglich, in der Web-Oberfläche nicht).
- Vorschau auf der Karte: Bahnen, Abschläge, Grüns, mit den Hinweisen der
  heutigen Vorschau (fehlende Löcher, fehlende Grünflächen).
- Speichern in der Bibliothek, aufs Gerät laden, löschen.
- Par und Lochzahl von Hand korrigieren. Das vollständige Erfassen auf der
  Karte (Abschläge und Grüns setzen) ist für eine spätere Version vorgemerkt,
  nicht für die erste.

### 3. Schläge auf der Karte

- Runde auf der Karte: jeder Schlag als Punkt, verbunden mit Linien, mit Weite
  und Schläger; Loch für Loch durchblättern.
- Schläge ohne GPS-Position werden in der Liste gezeigt, nicht auf der Karte.

### 4. Einstellungen für das Gerät

- **Schlägerauswahl:** welche Schläger im Bag sind, Name und Schlagweite.
  Heute ist die Liste mit elf Schlägern fest; die Firmware bekommt eine
  wählbare Liste (bis 14 Schläger). Die Schlägerwahl am Gerät zeigt dann nur
  die eigenen.
- Schlagweiten aus den gemessenen Durchschnitten übernehmen.
- Display: Helligkeit, Zeit bis zum Dimmen.
- Knöpfe 1 und 2 tauschen (je nach Tragweise).
- WLAN-Einrichtung für die Web-Oberfläche.

## Nötige Änderungen an der Firmware

Alles davon ist im Kern testbar, bevor die App existiert.

| Nr. | Änderung | Testbar ohne Board |
|---|---|---|
| F1 | Dauerhafte Kennung für Runden und Plätze, Änderungszähler für Plätze, Versionsnummer der Schnittstelle | ja |
| F2 | Wählbare Schlägerliste statt fester elf Schläger; Schlägerwahl am Gerät passt sich an | ja, inklusive Screenshots |
| F3 | Einstellungen erweitern (Helligkeit, Dimmzeit, Knöpfe tauschen) | ja |
| F4 | Nachrichten in Stücke zerlegen und zusammensetzen; Simulator spricht dasselbe über TCP | ja |
| F5 | Bluetooth-Dienst auf dem Gerät, Koppeln mit Code auf dem Display | nur kompilieren |

Das Dateiformat bleibt abwärtskompatibel: Alte Runden ohne Kennung erhalten
beim ersten Laden eine.

## Tests der App

| Art | Was sie prüft |
|---|---|
| Unit-Tests (Dart) | Distanzen, OpenStreetMap-Umwandlung, Statistik: gegen dieselben Testdaten wie C++ und JavaScript |
| Datenbank-Tests | Speichern, Laden, Migrationen, Export und Import |
| Abgleich gegen den Simulator | die echte Firmware-Logik als Gegenstelle: Runden holen, Platz senden, Abbruch mittendrin, zweimal abgleichen ändert nichts |
| Widget- und Bildtests | Ansichten mit festen Daten, auch in kleiner und grosser Schrift |
| Builds in GitHub Actions | Android-APK; iOS ohne Signatur |

Nicht automatisch prüfbar: die echte Bluetooth-Strecke und das Verhalten im
Hintergrund. Dafür gibt es eine kurze Checkliste für den Test mit Board.

## Schritte

Reihenfolge so, dass möglichst lange ohne Board gearbeitet werden kann.

**Firmware vorbereiten**

1. F1: Kennungen und Versionsnummer.
2. F2: wählbare Schlägerliste.
3. F3: weitere Einstellungen.
4. F4: Zerlegen und Zusammensetzen der Nachrichten, TCP-Zugang im Simulator.

**App**

5. Projektgerüst in `app/`, GitHub Actions (Analyse, Tests, Android-Build, iOS-Build ohne Signatur).
6. Datenmodell und lokale Datenbank, Export und Import.
7. Geräte-Client und Abgleich, getestet gegen den Simulator.
8. Runden: Liste, Detail, Verlauf und Schlägerstatistik.
9. Plätze: Suche, Bibliothek, Kartenvorschau, aufs Gerät laden.
10. Schläge auf der Karte.
11. Einstellungen und Schlägerauswahl.

**Mit Board**

12. F5: Bluetooth-Dienst in der Firmware.
13. Bluetooth in der App: Suchen, Koppeln, Abgleich; Test nach Checkliste.
14. Feinschliff, Installationsdatei für Android, Anleitung für iOS.

## Offene Punkte

- **Kartenbilder:** Für Golf sind Luftbilder am nützlichsten. Die freien
  OpenStreetMap-Kartenserver dürfen von Apps nur sehr eingeschränkt genutzt
  werden. Es braucht einen Anbieter mit eigenem Zugangsschlüssel (viele haben
  eine kostenlose Stufe). Das ist vor Schritt 9 zu entscheiden.
- **iOS:** Ohne Mac und Entwicklerkonto bleibt iOS auf „kompiliert in GitHub
  Actions“ beschränkt.
- **Entwicklungsumgebung:** Flutter ist in der jetzigen Cloud-Umgebung nicht
  installiert; die nötigen Download-Server müssen freigegeben werden. Das kläre
  ich zu Beginn von Schritt 5.
- **Bluetooth und WLAN gleichzeitig** kosten auf dem ESP32 Speicher und Strom.
  Ob beides parallel sinnvoll ist, zeigt sich am Board.
- **Firmware-Aktualisierung über die App** wäre praktisch, ist aber nicht Teil
  der ersten Version.
- **Reihenfolge mit Schritt 12 (Inbetriebnahme):** Die Schritte 1 bis 11 dieses
  Plans hängen nicht am Board. Trotzdem lohnt es sich, das Board zuerst in
  Betrieb zu nehmen, weil sich dabei Annahmen ändern können (vor allem beim GPS).
