#include "core/display_text.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace golf {

namespace {
constexpr int kMaxShownDistance = 999;

void formatDistance(char* out, size_t size, bool available, int meters) {
    if (!available) {
        std::snprintf(out, size, "---");
    } else {
        std::snprintf(out, size, "%d", meters > kMaxShownDistance ? kMaxShownDistance : meters);
    }
}
} // namespace

DistanceScreenText distanceScreenText(const App& app) {
    DistanceScreenText t;
    std::memset(&t, 0, sizeof(t));

    const int hole_index = app.currentHole();
    const Hole& hole = app.course().holes[hole_index];
    std::snprintf(t.title, sizeof(t.title), "LOCH %d", hole_index + 1);

    const int length = holeLengthMeters(hole);
    if (hole.par > 0 && length > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Par %d · %d m", hole.par, length);
    } else if (hole.par > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "Par %d", hole.par);
    } else if (length > 0) {
        std::snprintf(t.subtitle, sizeof(t.subtitle), "%d m", length);
    }

    t.gps_ok = app.hasPosition();
    const GreenDistances d = app.distances();
    formatDistance(t.back, sizeof(t.back), d.available, d.back);
    formatDistance(t.center, sizeof(t.center), d.available, d.center);
    formatDistance(t.front, sizeof(t.front), d.available, d.front);
    if (!t.gps_ok) {
        std::snprintf(t.hint, sizeof(t.hint), "Kein GPS-Empfang");
    } else if (!hole.has_green) {
        std::snprintf(t.hint, sizeof(t.hint), "Grün nicht markiert");
    }

    const int shots = app.round().shotCount(hole_index);
    if (shots == 0) {
        std::snprintf(t.foot, sizeof(t.foot), "Noch kein Schlag");
    } else {
        const char* word = shots == 1 ? "Schlag" : "Schläge";
        const ShotDistance last = app.lastShotDistance();
        if (last.available) {
            long meters = std::lround(last.meters);
            if (meters > kMaxShownDistance) meters = kMaxShownDistance;
            std::snprintf(t.foot, sizeof(t.foot), "%d %s · letzter %ld m", shots, word, meters);
        } else {
            std::snprintf(t.foot, sizeof(t.foot), "%d %s", shots, word);
        }
    }
    return t;
}

} // namespace golf
