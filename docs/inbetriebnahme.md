# Inbetriebnahme auf dem Gerät

Die Firmware ist vollständig, aber noch nie auf dem echten Board gelaufen.
Alles unterhalb der Hardware ist getestet (Kern, Oberfläche, Speicherlogik,
Handy-Schnittstelle). Was hier steht, lässt sich nur am Gerät prüfen.

## Flashen

```sh
pio run -e device -t upload      # Board per USB-C verbinden
pio device monitor               # serielle Ausgabe, 115200 Baud
```

Startet das Hochladen nicht: BOOT gedrückt halten, PWR kurz drücken, BOOT
loslassen, dann erneut hochladen.

Nach dem Start meldet die Firmware auf der seriellen Schnittstelle:

```
Golf-Tracker 0.1.0
Display ok, Touch ok, Speicher ok, Akku-Chip ok
```

## Annahmen über die Hardware

Alle Anschlüsse stehen in `src/device/board.h`. Sie stammen aus dem offiziellen
Repository `waveshareteam/ESP32-S3-Touch-AMOLED-1.75` (Datei `pin_config.h` und
`HARDWARE_REFERENCE.md`).

| Teil | Annahme | Sicherheit |
|---|---|---|
| Display | CO5300 über QSPI, Pins 4/5/6/7/38/12, Reset 39, Spaltenversatz 6 | aus dem Herstellerbeispiel übernommen |
| Touch | CST9217 an I2C-Adresse 0x5A, Reset 40 | aus dem Herstellerbeispiel; Ausrichtung der Achsen unbekannt |
| Knopf 1 | BOOT an GPIO0, gedrückt = 0 | laut Hersteller |
| Knopf 2 | PWR über den I/O-Baustein TCA9554 (0x20), Bit 4, gedrückt = 1 | laut Hersteller, ohne Beispielcode |
| GPS | LC76G über I2C (0x50 schreiben, 0x54 lesen) | Ablauf aus dem Herstellerbeispiel nachgebaut |
| Speicher | interner Flash mit LittleFS, nicht die SD-Karte | Standard des ESP32 |
| Akku | AXP2101 an I2C-Adresse 0x34, gelesen mit XPowersLib | aus dem Herstellerbeispiel übernommen |
| WLAN | Standardfunktionen des ESP32 | – |

## Reihenfolge der Prüfung

Jeden Punkt einzeln abhaken. Bei einem Fehler zuerst die serielle Ausgabe lesen.

1. **Display:** Nach dem Start erscheint die Distanz-Ansicht mit „LOCH 1“.
   - Bild verschoben oder mit Streifen am Rand: `kLcdColumnOffset` in `board.h` anpassen.
   - Schwarz, aber „Display ok“: Helligkeit in `display.cpp` prüfen (`setBrightness`).
   - „Display FEHLT“: Pins in `board.h` mit dem Aufdruck bzw. Schaltplan vergleichen.
2. **Knopf 1 (BOOT):** Kurz drücken öffnet die Schlägerwahl.
3. **Knopf 2 (PWR):** Lang drücken öffnet das Menü.
   - Reagiert nicht: Bit bzw. Pegel in `board.h` prüfen (`kExpanderPwrButtonBit`).
   - Achtung: Sehr langes Halten von PWR schaltet das Board über den Stromversorgungs-Chip aus.
4. **Touch:** In der Schlägerwahl einen Schläger antippen; in der Ansicht wischen.
   - Tippen trifft daneben oder gespiegelt: In `display.cpp` in `readTouch` die
     Koordinaten spiegeln (`465 - x`) oder vertauschen.
5. **GPS:** Im Freien mit angeschlossener Antenne; nach 30 bis 60 Sekunden wird
   der Punkt neben „LOCH“ grün.
   - Bleibt orange: Antenne und freie Sicht prüfen. Liefert das Modul gar nichts,
     ist die I2C-Anbindung zu prüfen (Abschnitt GPS unten).
6. **Speicher:** Einen Schlag zählen, Gerät aus- und einschalten: Der Schlag ist noch da.
7. **WLAN:** Menü → „Mit Handy verbinden“. Das Display zeigt WLAN-Name,
   Passwort und Adresse. Am Handy verbinden und die Adresse im Browser öffnen.
8. **Akku und Display:** Mit Akku steht oben der Ladestand, am USB-Kabel „lädt“.
   Nach 20 Sekunden ohne Eingabe dimmt das Display, ein Knopfdruck macht es wieder hell.
   - Dimmt nicht sichtbar: `setBrightness` wirkt beim CO5300 über Panel-Befehle;
     Werte in `lib/core/src/core/power.h` anpassen (`kBrightnessDim`).
9. **Platz laden:** Im Bereich „Gerät“ das eigene WLAN eintragen, „Mit Handy
   verbinden“ neu öffnen, dann im Bereich „Platz“ den Golfplatz suchen.

## GPS im Detail

Das Herstellerbeispiel liest das Modul über I2C, nicht über eine serielle
Leitung. Der Ablauf ist in `lib/core/src/core/lc76g.h` beschrieben und gegen
ein nachgebautes Modul getestet, aber nicht gegen das echte. Unklar ist:

- ob das Modul die Sätze GGA und RMC sendet (Standard beim LC76G),
- ob es Daten zwischen zwei Abfragen sammelt oder verwirft,
- ob der Reset-Anschluss (I/O-Baustein Bit 7) angesteuert werden muss.

Der Hersteller erwähnt zusätzlich eine mögliche serielle Verbindung über
GPIO17/GPIO18, die je nach Bestückung fehlen kann. Sie wird nicht verwendet.

## Bekannte Lücken

- Stromsparen betrifft bisher nur das Display (dimmen nach 20 Sekunden, aus
  nach 30 Minuten). Prozessor und GPS laufen durch; die Laufzeit mit Akku ist
  noch nicht gemessen.
- Die SD-Karte wird nicht benutzt.
- Die Web-Oberfläche ist nicht passwortgeschützt; der eigene Hotspot schon.
