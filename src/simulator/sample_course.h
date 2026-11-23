// Beispielplatz fuer den Simulator.
#pragma once

#include "core/course.h"

namespace sim {

// 18 Loecher (Par 72), alle mit Abschlag und Gruen. Jedes Loch verlaeuft vom
// Abschlag gerade nach Norden.
golf::Course sampleCourse();

// Position auf der Linie Abschlag-Gruen, `meters` vom Abschlag entfernt.
golf::LatLon positionOnHole(const golf::Hole& hole, double meters);

} // namespace sim
