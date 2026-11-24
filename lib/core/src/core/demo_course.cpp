#include "core/demo_course.h"

#include <cmath>
#include <cstring>

namespace golf {

namespace {
const int kPars[kMaxHoles] = {4, 5, 3, 4, 4, 3, 5, 4, 4, 4, 3, 5, 4, 4, 3, 4, 5, 4};
const int kLengths[kMaxHoles] = {342, 468, 151, 365, 318, 172, 487, 356, 381,
                                 334, 143, 502, 371, 348, 165, 389, 476, 360};
constexpr double kMetersPerDegLat = 111180.0; // bei 47 Grad Nord
constexpr double kPi = 3.14159265358979323846;
} // namespace

Course demoCourse() {
    Course course;
    std::strcpy(course.name, "Beispielplatz");
    for (int h = 0; h < kMaxHoles; h++) {
        Hole& hole = course.holes[h];
        hole.par = kPars[h];
        hole.has_tee = true;
        hole.tee = LatLon{47.3769, 8.5417 + h * 0.002};
        hole.has_green = true;
        hole.green_center = LatLon{hole.tee.lat + kLengths[h] / kMetersPerDegLat, hole.tee.lon};
    }
    return course;
}

LatLon positionOnHole(const Hole& hole, double meters, double meters_right) {
    const double length = distanceMeters(hole.tee, hole.green_center);
    if (length <= 0.0) return hole.tee;
    const double f = meters / length;
    const double d_lat = hole.green_center.lat - hole.tee.lat;
    const double d_lon = hole.green_center.lon - hole.tee.lon;
    LatLon p{hole.tee.lat + d_lat * f, hole.tee.lon + d_lon * f};

    if (meters_right != 0.0) {
        // Rechtwinklig zur Spielrichtung, in Metern gerechnet.
        const double meters_per_deg_lon = kMetersPerDegLat * std::cos(p.lat * kPi / 180.0);
        const double north = d_lat * kMetersPerDegLat / length; // Einheitsvektor der Spielrichtung
        const double east = d_lon * meters_per_deg_lon / length;
        p.lat += (-east * meters_right) / kMetersPerDegLat;
        p.lon += (north * meters_right) / meters_per_deg_lon;
    }
    return p;
}

} // namespace golf
