// Schlaeger, das Bag des Spielers und der Schlaegervorschlag.
#pragma once

#include <cstdint>

namespace golf {

// Alle Schlaeger, die das Geraet kennt. Welche davon der Spieler dabeihat,
// steht im Bag. Die Reihenfolge ist die der Anzeige (lang nach kurz).
enum class Club : uint8_t {
    Driver,
    Wood3,
    Wood5,
    Wood7,
    Hybrid,
    Iron3,
    Iron4,
    Iron5,
    Iron6,
    Iron7,
    Iron8,
    Iron9,
    PitchingWedge,
    GapWedge,
    SandWedge,
    LobWedge,
    Putter,
    None, // noch kein Schlaeger gewaehlt
};

constexpr int kClubCount = static_cast<int>(Club::None);
constexpr int kMaxBagClubs = 14; // mehr erlauben die Golfregeln nicht

// Kurzname fuer die Schlaegerwahl ("Dr", "E7") und voller Name ("Driver", "Eisen 7").
// Der Kurzname steht auch in den gespeicherten Runden und aendert sich deshalb nie.
const char* clubShortName(Club club);
const char* clubName(Club club);

// Schlaeger zu einem Kurznamen; Club::None, wenn es ihn nicht gibt.
Club clubFromShortName(const char* short_name);

// Das Bag: welche Schlaeger dabei sind und wie weit der Spieler damit schlaegt.
// Der Putter ist immer dabei und hat keine Schlagweite.
struct Bag {
    int carry_m[kClubCount] = {210, 190, 180, 170, 175, 180, 170, 160, 150, 140, 128, 116, 100, 85, 70, 55, 0};
    // Standard: Driver, Holz 3, Hybrid, Eisen 5 bis 9, Pitching Wedge, Sand Wedge, Putter
    bool in_bag[kClubCount] = {true,  true, false, false, true, false, false, true, true,
                               true,  true, true,  true,  false, true, false, true};
};

// Anzahl Schlaeger im Bag, Putter mitgezaehlt.
int bagSize(const Bag& bag);

// Gueltig ist ein Bag mit Putter, mindestens einem weiteren Schlaeger und
// hoechstens 14 Schlaegern.
bool isValidBag(const Bag& bag);

// Die Schlaeger im Bag in Anzeigereihenfolge. `out` fasst kMaxBagClubs
// Eintraege; zurueck kommt die Anzahl.
int bagClubs(const Bag& bag, Club* out);

constexpr int kPuttMaxDistanceM = 15;
constexpr int kDistanceUnknown = -1;

// Schlaegt einen Schlaeger aus dem Bag fuer den naechsten Schlag vor.
//
//   distance_m   Distanz zur Gruenmitte, kDistanceUnknown wenn das Gruen fehlt
//   first_shot   true beim Abschlag
//   par          Par des Lochs, 0 wenn unbekannt
//
// Regeln: bis 15 m der Putter; am Abschlag eines Par 4 oder 5 der laengste
// Schlaeger; sonst der Schlaeger, dessen Schlagweite der Distanz am naechsten
// liegt (bei Gleichstand der kuerzere). Ohne Distanz: der laengste am
// Abschlag, sonst ein mittlerer.
Club suggestClub(const Bag& bag, int distance_m, bool first_shot, int par);

// Naechster Schlaeger im Bag; nach dem letzten folgt wieder der erste.
Club nextClub(const Bag& bag, Club club);

} // namespace golf
