// Hilfen fuer Tests: Positionen in Metern relativ zu einem Ausgangspunkt.
#pragma once

#include "core/geo.h"

namespace testsupport {

// Abschlag des Testlochs (bei Zuerich).
constexpr golf::LatLon kTee{47.3769, 8.5417};

// Punkt `meters` noerdlich und `east` oestlich von `from`. Genau auf wenige Zentimeter.
inline golf::LatLon offset(const golf::LatLon& from, double north, double east = 0.0) {
    constexpr double kMetersPerDegLat = 111180.0; // bei 47 Grad Nord
    constexpr double kMetersPerDegLon = 75500.0;
    golf::LatLon p = from;
    // Zuerst grob, dann mit der echten Distanzfunktion nachskalieren.
    p.lat += north / kMetersPerDegLat;
    p.lon += east / kMetersPerDegLon;
    if (north != 0.0 && east == 0.0) {
        const double actual = golf::distanceMeters(from, p);
        p.lat = from.lat + (p.lat - from.lat) * (north < 0 ? -north : north) / actual;
    }
    return p;
}

} // namespace testsupport
