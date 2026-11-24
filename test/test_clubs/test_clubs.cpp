#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <set>
#include <string>

#include <unity.h>

#include "core/clubs.h"

using namespace golf;

void setUp() {}
void tearDown() {}

static const Bag kBag; // Standard-Bag

#define ASSERT_CLUB(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

static void expectClub(Club expected, int distance, bool first, int par) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Distanz %d, Abschlag %d, Par %d", distance, first, par);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>(suggestClub(kBag, distance, first, par)), msg);
}

// Bag mit genau diesen Schlaegern (der Putter kommt immer dazu).
static Bag bagWith(std::initializer_list<Club> clubs) {
    Bag bag;
    for (int i = 0; i < kClubCount; i++) bag.in_bag[i] = false;
    for (Club c : clubs) bag.in_bag[static_cast<int>(c)] = true;
    bag.in_bag[static_cast<int>(Club::Putter)] = true;
    return bag;
}

// ---------- Katalog ----------

static void test_catalog_has_17_clubs_with_unique_names() {
    TEST_ASSERT_EQUAL_INT(17, kClubCount);
    std::set<std::string> shorts, names;
    for (int i = 0; i < kClubCount; i++) {
        const Club c = static_cast<Club>(i);
        TEST_ASSERT_EQUAL_INT(2, static_cast<int>(std::strlen(clubShortName(c)))); // passt auf die Kachel
        TEST_ASSERT_TRUE(std::strlen(clubName(c)) <= 14);
        TEST_ASSERT_TRUE(shorts.insert(clubShortName(c)).second);
        TEST_ASSERT_TRUE(names.insert(clubName(c)).second);
    }
    TEST_ASSERT_EQUAL_STRING("Dr", clubShortName(Club::Driver));
    TEST_ASSERT_EQUAL_STRING("Eisen 7", clubName(Club::Iron7));
    TEST_ASSERT_EQUAL_STRING("Gap Wedge", clubName(Club::GapWedge));
    TEST_ASSERT_EQUAL_STRING("-", clubShortName(Club::None));
    TEST_ASSERT_EQUAL_STRING("-", clubName(Club::None));
}

static void test_short_names_of_the_first_version_are_unchanged() {
    // Diese Kuerzel stehen in bereits gespeicherten Runden.
    const struct {
        const char* code;
        Club club;
    } old[] = {{"Dr", Club::Driver}, {"H3", Club::Wood3}, {"Hy", Club::Hybrid}, {"E5", Club::Iron5},
               {"E6", Club::Iron6},  {"E7", Club::Iron7}, {"E8", Club::Iron8},  {"E9", Club::Iron9},
               {"PW", Club::PitchingWedge}, {"SW", Club::SandWedge}, {"Pt", Club::Putter}};
    for (const auto& entry : old) {
        ASSERT_CLUB(entry.club, clubFromShortName(entry.code));
        TEST_ASSERT_EQUAL_STRING(entry.code, clubShortName(entry.club));
    }
}

static void test_club_from_short_name() {
    for (int i = 0; i < kClubCount; i++) {
        ASSERT_CLUB(static_cast<Club>(i), clubFromShortName(clubShortName(static_cast<Club>(i))));
    }
    ASSERT_CLUB(Club::None, clubFromShortName("XX"));
    ASSERT_CLUB(Club::None, clubFromShortName(""));
    ASSERT_CLUB(Club::None, clubFromShortName("-"));
    ASSERT_CLUB(Club::None, clubFromShortName("dr")); // Gross-/Kleinschreibung zaehlt
    ASSERT_CLUB(Club::None, clubFromShortName(nullptr));
}

// ---------- Bag ----------

static void test_default_bag_is_the_set_of_the_first_version() {
    Club list[kMaxBagClubs];
    TEST_ASSERT_EQUAL_INT(11, bagClubs(kBag, list));
    TEST_ASSERT_EQUAL_INT(11, bagSize(kBag));
    const Club expected[] = {Club::Driver, Club::Wood3, Club::Hybrid, Club::Iron5, Club::Iron6, Club::Iron7,
                             Club::Iron8,  Club::Iron9, Club::PitchingWedge, Club::SandWedge, Club::Putter};
    for (int i = 0; i < 11; i++) ASSERT_CLUB(expected[i], list[i]);
    TEST_ASSERT_TRUE(isValidBag(kBag));
    TEST_ASSERT_EQUAL_INT(0, kBag.carry_m[static_cast<int>(Club::Putter)]);
    TEST_ASSERT_EQUAL_INT(210, kBag.carry_m[static_cast<int>(Club::Driver)]);
    TEST_ASSERT_EQUAL_INT(140, kBag.carry_m[static_cast<int>(Club::Iron7)]);
}

static void test_every_catalog_club_has_a_default_distance() {
    for (int i = 0; i < kClubCount; i++) {
        if (static_cast<Club>(i) == Club::Putter) continue;
        TEST_ASSERT_TRUE(kBag.carry_m[i] >= 40 && kBag.carry_m[i] <= 250);
    }
}

static void test_putter_is_always_in_the_bag() {
    Bag bag = bagWith({Club::Driver, Club::Iron7});
    bag.in_bag[static_cast<int>(Club::Putter)] = false; // auch wenn jemand ihn herausnimmt
    Club list[kMaxBagClubs];
    TEST_ASSERT_EQUAL_INT(3, bagClubs(bag, list));
    ASSERT_CLUB(Club::Putter, list[2]);
    TEST_ASSERT_EQUAL_INT(3, bagSize(bag));
    ASSERT_CLUB(Club::Putter, suggestClub(bag, 5, false, 4));
}

static void test_bag_size_limits() {
    TEST_ASSERT_FALSE(isValidBag(bagWith({})));             // nur Putter
    TEST_ASSERT_TRUE(isValidBag(bagWith({Club::Iron7})));   // Putter und ein Schlaeger
    Bag full;
    for (int i = 0; i < kClubCount; i++) full.in_bag[i] = true; // alle 17
    TEST_ASSERT_FALSE(isValidBag(full));
    full.in_bag[static_cast<int>(Club::Wood7)] = false;
    full.in_bag[static_cast<int>(Club::Iron3)] = false;
    TEST_ASSERT_FALSE(isValidBag(full)); // 15
    full.in_bag[static_cast<int>(Club::LobWedge)] = false;
    TEST_ASSERT_TRUE(isValidBag(full)); // 14
    Club list[kMaxBagClubs];
    TEST_ASSERT_EQUAL_INT(14, bagClubs(full, list));
}

static void test_bag_clubs_never_writes_more_than_14() {
    Bag all;
    for (int i = 0; i < kClubCount; i++) all.in_bag[i] = true;
    Club list[kMaxBagClubs + 2];
    list[kMaxBagClubs] = Club::None;
    TEST_ASSERT_EQUAL_INT(14, bagClubs(all, list));
    ASSERT_CLUB(Club::None, list[kMaxBagClubs]); // dahinter nichts ueberschrieben
}

// ---------- Vorschlag mit dem Standard-Bag ----------

static void test_putter_up_to_15_metres() {
    expectClub(Club::Putter, 0, false, 4);
    expectClub(Club::Putter, 15, false, 4);
    expectClub(Club::SandWedge, 16, false, 4);
    expectClub(Club::Putter, 5, true, 4); // auch als erster Schlag
}

static void test_longest_club_on_tee_of_par_4_and_5() {
    expectClub(Club::Driver, 350, true, 4);
    expectClub(Club::Driver, 480, true, 5);
    expectClub(Club::Driver, 250, true, 4);
}

static void test_par_3_tee_shot_goes_by_distance() {
    expectClub(Club::Iron6, 151, true, 3);
    expectClub(Club::Hybrid, 172, true, 3);
    expectClub(Club::Iron9, 118, true, 3);
}

static void test_closest_club_on_later_shots() {
    expectClub(Club::Iron7, 140, false, 4);
    expectClub(Club::Iron7, 143, false, 4);
    expectClub(Club::Iron6, 147, false, 4);
    expectClub(Club::PitchingWedge, 100, false, 4);
    expectClub(Club::SandWedge, 30, false, 4);
    expectClub(Club::Wood3, 195, false, 5);
    expectClub(Club::Driver, 9999, false, 5);
}

static void test_tie_goes_to_shorter_club() {
    expectClub(Club::Iron7, 145, false, 4); // zwischen Eisen 6 (150) und Eisen 7 (140)
    expectClub(Club::Wood3, 200, false, 5); // zwischen Driver (210) und Holz 3 (190)
}

static void test_unknown_distance() {
    expectClub(Club::Driver, kDistanceUnknown, true, 4);
    expectClub(Club::Driver, kDistanceUnknown, true, 0);
    expectClub(Club::Iron7, kDistanceUnknown, true, 3);
    expectClub(Club::Iron7, kDistanceUnknown, false, 4);
    expectClub(Club::Iron6, 151, true, 0); // Par unbekannt: nach Distanz
}

// ---------- Vorschlag mit eigenem Bag ----------

static void test_only_clubs_in_the_bag_are_suggested() {
    const Bag bag = bagWith({Club::Wood5, Club::Iron4, Club::Iron8, Club::GapWedge});
    ASSERT_CLUB(Club::Wood5, suggestClub(bag, 400, true, 4)); // laengster statt Driver
    ASSERT_CLUB(Club::Iron4, suggestClub(bag, 160, false, 4)); // Eisen 5 (160) ist nicht dabei
    ASSERT_CLUB(Club::Iron8, suggestClub(bag, 140, false, 4)); // Eisen 7 ist nicht dabei
    ASSERT_CLUB(Club::GapWedge, suggestClub(bag, 60, false, 4));
    ASSERT_CLUB(Club::Iron8, suggestClub(bag, kDistanceUnknown, false, 4)); // der mittlere
    ASSERT_CLUB(Club::Putter, suggestClub(bag, 10, false, 4));
}

static void test_every_suggestion_is_in_the_bag_for_many_bags() {
    // Alle Bags aus einer festen Zufallsfolge: Der Vorschlag ist immer ein
    // Schlaeger aus dem Bag, und der Putter kommt nie ueber 15 m.
    uint32_t state = 17;
    for (int round = 0; round < 300; round++) {
        Bag bag;
        int others = 0;
        for (int i = 0; i < kClubCount; i++) {
            state = state * 1664525u + 1013904223u;
            bag.in_bag[i] = ((state >> 16) % 3) != 0;
            bag.carry_m[i] = 30 + static_cast<int>((state >> 8) % 230);
            if (bag.in_bag[i] && static_cast<Club>(i) != Club::Putter) others++;
        }
        if (others == 0) bag.in_bag[static_cast<int>(Club::Iron7)] = true;
        for (int d = -1; d <= 400; d += 7) {
            for (int first = 0; first < 2; first++) {
                for (int par = 0; par <= 5; par++) {
                    const Club c = suggestClub(bag, d, first != 0, par);
                    TEST_ASSERT_TRUE(static_cast<int>(c) < kClubCount);
                    TEST_ASSERT_TRUE(c == Club::Putter || bag.in_bag[static_cast<int>(c)]);
                    if (d > kPuttMaxDistanceM) TEST_ASSERT_TRUE(c != Club::Putter);
                }
            }
        }
    }
}

static void test_distances_decide_not_the_catalog_order() {
    // Jemand schlaegt sein Hybrid weiter als sein Holz 3.
    Bag bag = bagWith({Club::Wood3, Club::Hybrid, Club::Iron7});
    bag.carry_m[static_cast<int>(Club::Wood3)] = 170;
    bag.carry_m[static_cast<int>(Club::Hybrid)] = 195;
    ASSERT_CLUB(Club::Hybrid, suggestClub(bag, 380, true, 4)); // laengster am Abschlag
    ASSERT_CLUB(Club::Hybrid, suggestClub(bag, 200, false, 5));
    ASSERT_CLUB(Club::Wood3, suggestClub(bag, 165, false, 4));
}

static void test_equal_distances_do_not_confuse_the_choice() {
    Bag bag = bagWith({Club::Iron6, Club::Iron7});
    bag.carry_m[static_cast<int>(Club::Iron6)] = 140;
    bag.carry_m[static_cast<int>(Club::Iron7)] = 140;
    const Club c = suggestClub(bag, 140, false, 4);
    TEST_ASSERT_TRUE(c == Club::Iron6 || c == Club::Iron7);
}

static void test_bag_with_only_a_putter_never_crashes() {
    const Bag bag = bagWith({});
    ASSERT_CLUB(Club::Putter, suggestClub(bag, 200, true, 4));
    ASSERT_CLUB(Club::Putter, suggestClub(bag, kDistanceUnknown, false, 4));
    ASSERT_CLUB(Club::Putter, nextClub(bag, Club::Putter));
}

// ---------- Weiterschalten ----------

static void test_next_club_cycles_through_the_bag_only() {
    Club list[kMaxBagClubs];
    const int count = bagClubs(kBag, list);
    Club c = list[0];
    for (int i = 1; i < count; i++) {
        c = nextClub(kBag, c);
        ASSERT_CLUB(list[i], c);
    }
    ASSERT_CLUB(Club::Driver, nextClub(kBag, c)); // nach dem Putter wieder vorne
    ASSERT_CLUB(Club::Hybrid, nextClub(kBag, Club::Wood3)); // Holz 5 und 7 sind nicht im Bag
}

static void test_next_club_from_a_club_outside_the_bag_or_none() {
    ASSERT_CLUB(Club::Hybrid, nextClub(kBag, Club::Wood5)); // der naechste, der dabei ist
    ASSERT_CLUB(Club::Driver, nextClub(kBag, Club::None));
    const Bag small = bagWith({Club::Iron7});
    ASSERT_CLUB(Club::Putter, nextClub(small, Club::Iron7));
    ASSERT_CLUB(Club::Iron7, nextClub(small, Club::Putter));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_has_17_clubs_with_unique_names);
    RUN_TEST(test_short_names_of_the_first_version_are_unchanged);
    RUN_TEST(test_club_from_short_name);
    RUN_TEST(test_default_bag_is_the_set_of_the_first_version);
    RUN_TEST(test_every_catalog_club_has_a_default_distance);
    RUN_TEST(test_putter_is_always_in_the_bag);
    RUN_TEST(test_bag_size_limits);
    RUN_TEST(test_bag_clubs_never_writes_more_than_14);
    RUN_TEST(test_putter_up_to_15_metres);
    RUN_TEST(test_longest_club_on_tee_of_par_4_and_5);
    RUN_TEST(test_par_3_tee_shot_goes_by_distance);
    RUN_TEST(test_closest_club_on_later_shots);
    RUN_TEST(test_tie_goes_to_shorter_club);
    RUN_TEST(test_unknown_distance);
    RUN_TEST(test_only_clubs_in_the_bag_are_suggested);
    RUN_TEST(test_every_suggestion_is_in_the_bag_for_many_bags);
    RUN_TEST(test_distances_decide_not_the_catalog_order);
    RUN_TEST(test_equal_distances_do_not_confuse_the_choice);
    RUN_TEST(test_bag_with_only_a_putter_never_crashes);
    RUN_TEST(test_next_club_cycles_through_the_bag_only);
    RUN_TEST(test_next_club_from_a_club_outside_the_bag_or_none);
    return UNITY_END();
}
