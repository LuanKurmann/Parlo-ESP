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

} // namespace golf
