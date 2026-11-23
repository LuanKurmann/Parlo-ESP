#include "sample_course.h"

#include <cstring>

namespace sim {

namespace {
const int kPars[golf::kMaxHoles] = {4, 5, 3, 4, 4, 3, 5, 4, 4, 4, 3, 5, 4, 4, 3, 4, 5, 4};
const int kLengths[golf::kMaxHoles] = {342, 468, 151, 365, 318, 172, 487, 356, 381,
                                       334, 143, 502, 371, 348, 165, 389, 476, 360};
constexpr double kMetersPerDegLat = 111180.0; // bei 47 Grad Nord
} // namespace

golf::Course sampleCourse() {
    golf::Course course;
    std::strcpy(course.name, "Beispielplatz");
    for (int h = 0; h < golf::kMaxHoles; h++) {
        golf::Hole& hole = course.holes[h];
        hole.par = kPars[h];
        hole.has_tee = true;
        hole.tee = golf::LatLon{47.3769, 8.5417 + h * 0.002};
        hole.has_green = true;
        hole.green_center = golf::LatLon{hole.tee.lat + kLengths[h] / kMetersPerDegLat, hole.tee.lon};
    }
    return course;
}

golf::LatLon positionOnHole(const golf::Hole& hole, double meters) {
    const double length = golf::distanceMeters(hole.tee, hole.green_center);
    if (length <= 0.0) return hole.tee;
    const double f = meters / length;
    return golf::LatLon{hole.tee.lat + (hole.green_center.lat - hole.tee.lat) * f,
                        hole.tee.lon + (hole.green_center.lon - hole.tee.lon) * f};
}

} // namespace sim
