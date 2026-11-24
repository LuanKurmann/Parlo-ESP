// Erzeugt NMEA-Saetze, so wie sie ein GPS-Empfaenger sendet. Dient dem
// Simulator und den Tests als kuenstlicher Empfaenger.
#pragma once

#include <string>

#include "core/geo.h"

namespace golf {

struct NmeaFixInfo {
    bool fix = true;          // false: der Empfaenger hat (noch) keine Position
    LatLon position{0.0, 0.0};
    int satellites = 12;
    double hdop = 0.9;
    double altitude_m = 430.0;
    double speed_kmh = 0.0;

    bool has_time = true;     // false wie direkt nach dem Einschalten
    int hour = 8, minute = 0, second = 0;
    int year = 2026, month = 11, day = 24;
};

// Vollstaendiger Satz aus dem Inhalt: "$" + Inhalt + "*" + Pruefsumme + CR LF.
std::string nmeaSentence(const std::string& body);

std::string nmeaGga(const NmeaFixInfo& info);
std::string nmeaRmc(const NmeaFixInfo& info);

// Alles, was ein Empfaenger in einer Sekunde sendet: GGA, RMC und dazu
// Satellitenangaben (GSA, GSV), die die App nicht auswertet.
std::string nmeaBurst(const NmeaFixInfo& info);

} // namespace golf
