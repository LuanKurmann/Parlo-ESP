// Eine Golfrunde: Schlaege pro Loch, Score, Putts und Schlagweiten.
#pragma once

#include <cstdint>

#include "core/clubs.h"
#include "core/course.h"
#include "core/geo.h"

namespace golf {

constexpr int kMaxShotsPerHole = 20;

struct Shot {
    bool has_position = false; // false, wenn beim Schlag kein GPS-Empfang da war
    LatLon position{0.0, 0.0};
    Club club = Club::None;
};

class Round {
public:
    explicit Round(int hole_count = kMaxHoles);

    int holeCount() const { return hole_count_; }

    // Zaehlt einen Schlag. Ein bereits abgeschlossenes Loch wird dadurch wieder
    // geoeffnet. Liefert false, wenn das Loch ungueltig oder voll ist.
    bool addShot(int hole, const LatLon& position);
    bool addShotWithoutPosition(int hole);

    // Setzt den Schlaeger eines Schlags. Liefert false bei ungueltigem Index.
    bool setClub(int hole, int shot, Club club);

    // Loescht den letzten Schlag des Lochs. Liefert false, wenn es keinen gibt.
    bool removeLastShot(int hole);

    // Schliesst das Loch ab. Liefert false, wenn noch kein Schlag erfasst ist.
    bool finishHole(int hole);

    bool isFinished(int hole) const;
    int shotCount(int hole) const;
    int putts(int hole) const; // Schlaege mit dem Putter
    const Shot* shot(int hole, int index) const; // nullptr bei ungueltigem Index

private:
    struct HoleData {
        Shot shots[kMaxShotsPerHole];
        uint8_t count = 0;
        bool finished = false;
    };

    bool validHole(int hole) const { return hole >= 0 && hole < hole_count_; }
    bool pushShot(int hole, const Shot& shot);

    int hole_count_;
    HoleData holes_[kMaxHoles];
};

// ---------- Auswertung ----------

struct ShotDistance {
    bool available = false;
    double meters = 0.0;
};

// Weite eines Schlags: bis zur Position des naechsten Schlags. Beim letzten
// Schlag eines abgeschlossenen Lochs bis zur Gruenmitte, beim letzten Schlag
// eines laufenden Lochs bis zur aktuellen Position des Spielers (player, darf
// nullptr sein). Putts und Schlaege ohne Position haben keine Weite.
ShotDistance shotDistance(const Round& round, const Course& course, int hole, int shot, const LatLon* player);

struct RoundTotals {
    int holes_played = 0;        // abgeschlossene Loecher
    int strokes = 0;             // Schlaege auf abgeschlossenen Loechern
    int putts = 0;
    int to_par = 0;              // Schlaege minus Par, nur Loecher mit bekanntem Par
    int holes_with_par = 0;
    double longest_shot_m = 0.0;
};

RoundTotals roundTotals(const Round& round, const Course& course);

enum class ScoreName : uint8_t { Unknown, HoleInOne, Albatross, Eagle, Birdie, Par, Bogey, DoubleBogey, Worse };

// Bezeichnung des Ergebnisses eines Lochs. Unknown bei Par 0 oder 0 Schlaegen.
ScoreName scoreName(int strokes, int par);

} // namespace golf
