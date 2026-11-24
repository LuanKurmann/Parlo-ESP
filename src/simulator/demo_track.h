// Erzeugt eine Beispielaufzeichnung: ein Spieler geht die ersten Loecher des
// Beispielplatzes ab und bleibt an seinen Schlagpositionen stehen.
#pragma once

#include <string>

namespace sim {

// NMEA-Text, eine Messung pro Sekunde, mit leichter GPS-Streuung. Die ersten
// Sekunden hat der Empfaenger noch keine Position (wie nach dem Einschalten).
std::string demoTrack(int holes = 3);

} // namespace sim
