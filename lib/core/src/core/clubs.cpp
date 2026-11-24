#include "core/clubs.h"

#include <cstdlib>
#include <cstring>

namespace golf {

namespace {

const char* const kShortNames[kClubCount] = {"Dr", "H3", "H5", "H7", "Hy", "E3", "E4", "E5", "E6",
                                             "E7", "E8", "E9", "PW", "GW", "SW", "LW", "Pt"};
const char* const kNames[kClubCount] = {"Driver",  "Holz 3",  "Holz 5",  "Holz 7",         "Hybrid",    "Eisen 3",
                                        "Eisen 4", "Eisen 5", "Eisen 6", "Eisen 7",        "Eisen 8",   "Eisen 9",
                                        "Pitching Wedge", "Gap Wedge", "Sand Wedge", "Lob Wedge", "Putter"};

constexpr int kPutter = static_cast<int>(Club::Putter);

bool inBag(const Bag& bag, int index) { return index == kPutter || bag.in_bag[index]; }

// Der Schlaeger im Bag mit der groessten Schlagweite.
Club longestClub(const Bag& bag) {
    int best = -1;
    for (int i = 0; i < kClubCount; i++) {
        if (i == kPutter || !bag.in_bag[i]) continue;
        if (best < 0 || bag.carry_m[i] > bag.carry_m[best]) best = i;
    }
    return best < 0 ? Club::Putter : static_cast<Club>(best);
}

// Der Schlaeger im Bag, dessen Schlagweite der Distanz am naechsten liegt.
Club closestClub(const Bag& bag, int distance_m) {
    int best = -1;
    int best_gap = 0;
    for (int i = 0; i < kClubCount; i++) {
        if (i == kPutter || !bag.in_bag[i]) continue;
        const int gap = std::abs(bag.carry_m[i] - distance_m);
        // Bei gleichem Abstand gewinnt der kuerzere Schlaeger.
        if (best < 0 || gap < best_gap || (gap == best_gap && bag.carry_m[i] < bag.carry_m[best])) {
            best = i;
            best_gap = gap;
        }
    }
    return best < 0 ? Club::Putter : static_cast<Club>(best);
}

} // namespace

const char* clubShortName(Club club) {
    const int i = static_cast<int>(club);
    return i < kClubCount ? kShortNames[i] : "-";
}

const char* clubName(Club club) {
    const int i = static_cast<int>(club);
    return i < kClubCount ? kNames[i] : "-";
}

Club clubFromShortName(const char* short_name) {
    if (short_name == nullptr) return Club::None;
    for (int i = 0; i < kClubCount; i++) {
        if (std::strcmp(short_name, kShortNames[i]) == 0) return static_cast<Club>(i);
    }
    return Club::None;
}

int bagSize(const Bag& bag) {
    int count = 0;
    for (int i = 0; i < kClubCount; i++) {
        if (inBag(bag, i)) count++;
    }
    return count;
}

bool isValidBag(const Bag& bag) {
    const int size = bagSize(bag);
    return size >= 2 && size <= kMaxBagClubs;
}

int bagClubs(const Bag& bag, Club* out) {
    int count = 0;
    for (int i = 0; i < kClubCount && count < kMaxBagClubs; i++) {
        if (inBag(bag, i)) out[count++] = static_cast<Club>(i);
    }
    return count;
}

Club suggestClub(const Bag& bag, int distance_m, bool first_shot, int par) {
    const bool long_hole = par == 0 || par > 3;
    if (distance_m < 0) {
        if (first_shot && long_hole) return longestClub(bag);
        return closestClub(bag, 140); // ein mittlerer Schlaeger
    }
    if (distance_m <= kPuttMaxDistanceM) return Club::Putter;
    if (first_shot && par > 3) return longestClub(bag);
    return closestClub(bag, distance_m);
}

Club nextClub(const Bag& bag, Club club) {
    const int start = static_cast<int>(club) < kClubCount ? static_cast<int>(club) : kClubCount - 1;
    for (int step = 1; step <= kClubCount; step++) {
        const int i = (start + step) % kClubCount;
        if (inBag(bag, i)) return static_cast<Club>(i);
    }
    return Club::Putter;
}

} // namespace golf
