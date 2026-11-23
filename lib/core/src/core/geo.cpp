#include "core/geo.h"

#include <cmath>

namespace golf {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kSemiMajorAxis = 6378137.0;         // WGS84, Meter
constexpr double kEccentricitySq = 6.69437999014e-3; // WGS84
} // namespace

double distanceMeters(const LatLon& a, const LatLon& b) {
    const double mid_lat = (a.lat + b.lat) * 0.5 * kDegToRad;
    const double d_lat = (b.lat - a.lat) * kDegToRad;

    // Laengendifferenz ueber die Datumsgrenze hinweg auf [-180, 180] bringen.
    double d_lon_deg = b.lon - a.lon;
    while (d_lon_deg > 180.0) d_lon_deg -= 360.0;
    while (d_lon_deg < -180.0) d_lon_deg += 360.0;
    const double d_lon = d_lon_deg * kDegToRad;

    const double sin_lat = std::sin(mid_lat);
    const double w = 1.0 - kEccentricitySq * sin_lat * sin_lat;
    const double sqrt_w = std::sqrt(w);
    const double radius_north = kSemiMajorAxis * (1.0 - kEccentricitySq) / (w * sqrt_w);
    const double radius_east = kSemiMajorAxis / sqrt_w;

    const double north = d_lat * radius_north;
    const double east = d_lon * radius_east * std::cos(mid_lat);
    return std::sqrt(north * north + east * east);
}

} // namespace golf
