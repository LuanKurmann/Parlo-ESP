// Beispielplatz fuer Simulator und Tests.
#pragma once

#include "core/course.h"

namespace golf {

// 18 Loecher (Par 72), alle mit Abschlag und Gruen. Jedes Loch verlaeuft vom
// Abschlag gerade nach Norden; die Abschlaege liegen rund 150 m nebeneinander.
Course demoCourse();

// Position auf der Linie Abschlag-Gruen, `meters` vom Abschlag entfernt und
// `meters_right` seitlich versetzt (positiv = rechts in Spielrichtung).
LatLon positionOnHole(const Hole& hole, double meters, double meters_right = 0.0);

} // namespace golf
