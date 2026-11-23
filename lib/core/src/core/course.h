// Platzdaten: Loecher mit Par und Position des Gruens.
#pragma once

#include "core/geo.h"

namespace golf {

constexpr int kMaxHoles = 18;
constexpr float kDefaultGreenRadiusM = 13.0f;

struct Hole {
    int par = 0;              // 0 = unbekannt
    bool has_green = false;   // false, solange das Gruen weder importiert noch markiert ist
    LatLon green_center{0.0, 0.0};
    float green_radius_m = kDefaultGreenRadiusM; // halbe Tiefe des Gruens in Spielrichtung
    bool has_tee = false;
    LatLon tee{0.0, 0.0};
};

struct Course {
    char name[32] = "";
    int hole_count = kMaxHoles;
    Hole holes[kMaxHoles];
};

// Distanzen vom Spieler zum Gruen in ganzen Metern.
struct GreenDistances {
    bool available = false; // false, wenn das Gruen des Lochs nicht bekannt ist
    int front = 0;          // naechster Punkt des Gruens
    int center = 0;
    int back = 0;           // entferntester Punkt des Gruens
};

// Das Gruen wird als Kreis um die Mitte angenommen. Steht der Spieler auf dem
// Gruen, ist "front" die Distanz zum Rand hinter ihm.
GreenDistances greenDistances(const Hole& hole, const LatLon& player);

// Speichert die aktuelle Position als Gruenmitte ("Gruen hier markieren").
void markGreen(Hole& hole, const LatLon& position);

// Laenge des Lochs vom Abschlag zur Gruenmitte in Metern, 0 wenn unbekannt.
int holeLengthMeters(const Hole& hole);

} // namespace golf
