// Schlaeger und Schlaegervorschlag.
#pragma once

#include <cstdint>

namespace golf {

enum class Club : uint8_t {
    Driver,
    Wood3,
    Hybrid,
    Iron5,
    Iron6,
    Iron7,
    Iron8,
    Iron9,
    PitchingWedge,
    SandWedge,
    Putter,
    None, // noch kein Schlaeger gewaehlt
};

constexpr int kClubCount = static_cast<int>(Club::None);

// Kurzname fuer die Schlaegerwahl ("Dr", "E7") und voller Name ("Driver", "Eisen 7").
const char* clubShortName(Club club);
const char* clubName(Club club);

// Die Schlagweiten des Spielers pro Schlaeger in Metern. Der Putter hat 0.
struct Bag {
    int carry_m[kClubCount] = {210, 190, 175, 160, 150, 140, 128, 116, 100, 70, 0};
};

constexpr int kPuttMaxDistanceM = 15;
constexpr int kDistanceUnknown = -1;

// Schlaegt einen Schlaeger fuer den naechsten Schlag vor.
//
//   distance_m   Distanz zur Gruenmitte, kDistanceUnknown wenn das Gruen fehlt
//   first_shot   true beim Abschlag
//   par          Par des Lochs, 0 wenn unbekannt
//
// Regeln: bis 15 m der Putter; am Abschlag eines Par 4 oder 5 der Driver;
// sonst der Schlaeger, dessen Schlagweite der Distanz am naechsten liegt
// (bei Gleichstand der kuerzere). Ohne Distanz: Driver am Abschlag, sonst Eisen 7.
Club suggestClub(const Bag& bag, int distance_m, bool first_shot, int par);

// Naechster Schlaeger in der Auswahl; nach dem Putter folgt wieder der Driver.
Club nextClub(Club club);

} // namespace golf
