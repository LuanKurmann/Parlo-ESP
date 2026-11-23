#include "core/course.h"

#include <cmath>

namespace golf {

GreenDistances greenDistances(const Hole& hole, const LatLon& player) {
    GreenDistances result;
    if (!hole.has_green) return result;

    const double center = distanceMeters(player, hole.green_center);
    const double radius = hole.green_radius_m > 0.0f ? hole.green_radius_m : 0.0;
    result.available = true;
    result.center = static_cast<int>(std::lround(center));
    result.front = static_cast<int>(std::lround(std::fabs(center - radius)));
    result.back = static_cast<int>(std::lround(center + radius));
    return result;
}

void markGreen(Hole& hole, const LatLon& position) {
    hole.green_center = position;
    hole.has_green = true;
}

int holeLengthMeters(const Hole& hole) {
    if (!hole.has_green || !hole.has_tee) return 0;
    return static_cast<int>(std::lround(distanceMeters(hole.tee, hole.green_center)));
}

} // namespace golf
