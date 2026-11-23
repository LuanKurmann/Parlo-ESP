#include "core/clubs.h"

#include <cstdlib>

namespace golf {

namespace {
const char* const kShortNames[kClubCount] = {"Dr", "H3", "Hy", "E5", "E6", "E7", "E8", "E9", "PW", "SW", "Pt"};
const char* const kNames[kClubCount] = {"Driver",  "Holz 3",  "Hybrid",         "Eisen 5",    "Eisen 6", "Eisen 7",
                                        "Eisen 8", "Eisen 9", "Pitching Wedge", "Sand Wedge", "Putter"};
} // namespace

const char* clubShortName(Club club) {
    const int i = static_cast<int>(club);
    return i < kClubCount ? kShortNames[i] : "-";
}

const char* clubName(Club club) {
    const int i = static_cast<int>(club);
    return i < kClubCount ? kNames[i] : "-";
}

Club suggestClub(const Bag& bag, int distance_m, bool first_shot, int par) {
    const bool long_hole = par == 0 || par > 3;
    if (distance_m < 0) return (first_shot && long_hole) ? Club::Driver : Club::Iron7;
    if (distance_m <= kPuttMaxDistanceM) return Club::Putter;
    if (first_shot && par > 3) return Club::Driver;

    int best = 0;
    int best_gap = -1;
    for (int i = 0; i < kClubCount; i++) {
        if (static_cast<Club>(i) == Club::Putter) continue;
        const int gap = std::abs(bag.carry_m[i] - distance_m);
        // "<=": bei Gleichstand gewinnt der spaetere, also kuerzere Schlaeger.
        if (best_gap < 0 || gap <= best_gap) {
            best = i;
            best_gap = gap;
        }
    }
    return static_cast<Club>(best);
}

Club nextClub(Club club) {
    const int i = static_cast<int>(club);
    if (i >= kClubCount - 1) return Club::Driver;
    return static_cast<Club>(i + 1);
}

} // namespace golf
