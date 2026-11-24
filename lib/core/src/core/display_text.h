// Texte fuer die Anzeige. Reine Aufbereitung des App-Zustands, ohne LVGL,
// damit sie auf dem PC testbar ist.
#pragma once

#include "core/app.h"

namespace golf {

struct DistanceScreenText {
    char title[24];    // "LOCH 7"
    char subtitle[32]; // "Par 5 · 487 m", "Par 5" oder leer
    char back[8];      // Distanz zum hinteren Rand, "---" wenn unbekannt
    char center[8];
    char front[8];
    char foot[56];     // "2 Schläge · letzter 143 m"
    char hint[32];     // Grund, wenn keine Distanz da ist; sonst leer
    bool gps_ok;
};

DistanceScreenText distanceScreenText(const App& app);

// Faerbung eines Ergebnisses: unter Par, Par (oder unbekannt), ueber Par,
// oder noch kein Ergebnis.
enum class ScoreTone : uint8_t { None, Under, Par, Over };

// ---------- Ansicht "Schlaege" ----------

struct ShotRowText {
    char number[12];   // "1"
    char club[20];     // "Eisen 7", "–" solange keiner gewaehlt ist
    char distance[12]; // "218 m", "Putt" oder leer
};

struct ShotsScreenText {
    char title[24]; // "LOCH 7"
    int count;
    ShotRowText rows[kMaxShotsPerHole];
};

ShotsScreenText shotsScreenText(const App& app);

// ---------- Ansicht "Scorecard" ----------

struct ScoreCellText {
    char hole[12]; // "7"
    char score[12]; // "5", "–" ohne Schlag
    ScoreTone tone;
    bool current;  // das Loch, auf dem man gerade steht
};

struct ScorecardText {
    char subtitle[32]; // "6 Löcher gespielt"
    int hole_count;
    ScoreCellText cells[kMaxHoles];
    char strokes[20];  // "27 Schläge"
    char to_par[8];    // "+4", "E", "-2", "–" ohne Par
    ScoreTone to_par_tone;
    char putts[16];    // "11 Putts"
};

ScorecardText scorecardText(const App& app);

// ---------- Overlays ----------

struct ClubPickerText {
    char subtitle[40]; // "Noch 269 m bis Grünmitte" oder leer
    char hint[40];     // "Driver vorgeschlagen"
    int suggested;     // Index des markierten Schlaegers
};

ClubPickerText clubPickerText(const App& app);

const char* menuItemText(MenuItem item);

struct HoleDoneText {
    char caption[48]; // "LOCH 7 · PAR 5"
    char strokes[12];
    char name[16];    // "Bogey", leer ohne Par
    ScoreTone tone;
    char putts[16];   // "2 Putts"
    char hint[48];    // "Knopf drücken für Loch 8"
};

HoleDoneText holeDoneText(const App& app);

struct RoundSummaryText {
    char strokes[12];
    char line[40];   // "+4 nach 6 Löchern"
    ScoreTone tone;
    char detail[48]; // "11 Putts · längster Schlag 244 m"
    char hint[48];
};

RoundSummaryText roundSummaryText(const App& app);

// Ansicht "Mit Handy verbinden": was am Handy zu tun ist.
struct PhoneText {
    char line1[40]; // "Am Handy dieses WLAN wählen:"
    char value1[40]; // Name des WLANs
    char detail1[32]; // "Passwort 48213975" beim eigenen Hotspot, sonst leer
    char line2[40]; // "Dann im Browser öffnen:"
    char value2[48]; // Adresse
    char hint[40];
};

PhoneText phoneText(const App& app);

// Akkuanzeige oben im Display: "87 %", "lädt 87 %" oder leer, wenn unbekannt.
struct BatteryText {
    char text[16];
    bool low; // 20 % oder weniger und nicht am Laden
};

BatteryText batteryText(const App& app);

// Text einer Rueckmeldung, leer fuer Message::None.
const char* messageText(Message message);

} // namespace golf
