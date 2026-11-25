# Bluetooth-Protokoll

Wie die App mit dem Golf-Tracker spricht. Über Bluetooth laufen dieselben
Anfragen wie über WLAN (`/api/...`, beschrieben in `lib/core/src/core/api.h`),
nur anders verpackt, weil ein Bluetooth-Paket klein ist.

Stand: Zerlegen, Zusammensetzen und die Gegenstelle sind im Kern umgesetzt und
getestet (`lib/core/src/core/link.h`). Der Simulator bietet das Protokoll über
TCP an. Der Bluetooth-Dienst auf dem Gerät selbst fehlt noch.

## Dienst

| | Kennung | Richtung |
|---|---|---|
| Dienst | `7d2f0001-5a3b-4c6e-9b1d-3f0a6c8e4b21` | |
| Anfrage | `7d2f0002-5a3b-4c6e-9b1d-3f0a6c8e4b21` | App schreibt |
| Antwort | `7d2f0003-5a3b-4c6e-9b1d-3f0a6c8e4b21` | Gerät meldet (Benachrichtigung) |

## Nachricht

Eine Nachricht ist Text in UTF-8 mit zwei Kopfzeilen, getrennt durch einen
Zeilenumbruch (`\n`):

```
Anfrage   <nr>\n<METHODE> <pfad>\n<inhalt>
Antwort   <nr>\n<status>\n<inhalt>
```

Beispiel:

```
7\nGET /api/status\n
7\n200\n{"version":"0.1.0","api":3,...}
```

- **Nummer:** wählt die App, höchstens neun Ziffern. Die Antwort trägt dieselbe.
- **Methode:** `GET`, `PUT` oder `DELETE`.
- **Inhalt:** das JSON der Schnittstelle; darf selbst Zeilenumbrüche enthalten.
- **Status:** wie bei HTTP (200, 400, 404, 405, 413, 500).

Es läuft immer nur eine Anfrage. Die App wartet auf die Antwort, bevor sie die
nächste schickt.

## Stücke

Jede Nachricht wird in Stücke zerlegt, die in ein Paket passen. Mehrbyte-Werte
stehen mit dem niederwertigen Byte zuerst.

| Byte | Inhalt |
|---|---|
| 0 | Art: `1` erstes Stück, `2` weiteres Stück |
| 1–2 | laufende Nummer des Stücks, beginnend bei 0 |
| 3–6 | nur im ersten Stück: Länge der ganzen Nachricht in Bytes |
| 7–10 | nur im ersten Stück: CRC-32 der ganzen Nachricht |
| danach | Nutzdaten |

Die Nachricht `hallo` in einem einzigen Stück:

```
01 00 00  05 00 00 00  D1 31 72 B9  68 61 6C 6C 6F
```

- Ein Stück ist mindestens 20 Bytes gross (ausser dem letzten) und höchstens
  so gross, wie die Verbindung erlaubt.
- Eine Nachricht ist höchstens 128 kB gross, der Inhalt einer Anfrage höchstens 64 kB.
- CRC-32 ist die übliche Variante (wie bei ZIP), Prüfwert für `123456789`: `CBF43926`.

## Fehler

Der Empfänger verwirft eine Nachricht, wenn ein Stück fehlt, doppelt oder
ausser der Reihe kommt, wenn mehr Daten kommen als angekündigt oder wenn die
Prüfsumme nicht stimmt.

- Das Gerät antwortet dann **einmal** mit Nummer `0` und Status `400`.
- Dasselbe gilt für eine vollständig übertragene, aber unverständliche Anfrage.
- Ein neues erstes Stück beginnt immer von vorn, auch mitten in einer anderen
  Nachricht. Eine noch nicht abgeholte Antwort verfällt dabei.
- Nach einer Trennung der Verbindung ist alles Angefangene verworfen.

Die App sollte eine Anfrage nach einer Zeitüberschreitung oder Antwort mit
Nummer `0` wiederholen. Alle Anfragen ausser `PUT /api/course` lassen sich
gefahrlos wiederholen; ein wiederholtes `PUT /api/course` ersetzt den Platz
erneut und archiviert dabei eine inzwischen begonnene Runde.

## Im Simulator ausprobieren

```sh
.pio/build/simulator/program --headless --link 9000 --link-packet 180
```

Der Simulator lauscht dann auf `127.0.0.1:9000`. Weil TCP keine Paketgrenzen
kennt, stehen dort vor jedem Stück zwei Bytes mit seiner Länge. Der Simulator
sendet Stücke mit höchstens `--link-packet` Bytes, so wie es über Bluetooth
wäre. Es gibt immer nur eine Verbindung; eine neue ersetzt die bisherige.

Ein vollständiger Client in JavaScript liegt in `tools/link/link_client.js`,
die zugehörigen Tests in `tools/link/link.test.js`. Er dient als Vorlage für
die App.

## Typischer Abgleich

```
GET /api/status                    Version der Schnittstelle, Kennung des Geräts
GET /api/rounds                    Liste mit Kennungen
GET /api/rounds/id/<kennung>       jede Runde, die der App fehlt
DELETE /api/rounds/id/<kennung>    erst, wenn die Runde sicher gespeichert ist
GET /api/course                    Platz mit Kennung und Änderungszähler
PUT /api/settings                  Bag und Einstellungen aus der App
```
