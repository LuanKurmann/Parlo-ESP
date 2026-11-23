// Positionen und Distanzen auf der Erde.
#pragma once

namespace golf {

// Position in Dezimalgrad (WGS84). Sueden und Westen sind negativ.
struct LatLon {
    double lat;
    double lon;
};

// Distanz zwischen zwei Positionen in Metern.
//
// Rechnet auf dem WGS84-Ellipsoid mit den Kruemmungsradien der mittleren Breite.
// Auf Golf-Distanzen (bis einige Kilometer) liegt der Fehler unter 1 cm pro
// 100 m. Fuer Strecken ueber etwa 20 km ist die Funktion nicht gedacht.
double distanceMeters(const LatLon& a, const LatLon& b);

} // namespace golf
