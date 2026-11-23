#include <cstdio>
#include <cstring>

#include <unity.h>

#include "core/clubs.h"

using namespace golf;

void setUp() {}
void tearDown() {}

static const Bag kBag;

static void expectClub(Club expected, int distance, bool first, int par) {
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Distanz %d, Abschlag %d, Par %d", distance, first, par);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>(suggestClub(kBag, distance, first, par)), msg);
}

static void test_eleven_clubs_with_names() {
    TEST_ASSERT_EQUAL_INT(11, kClubCount);
    TEST_ASSERT_EQUAL_STRING("Dr", clubShortName(Club::Driver));
    TEST_ASSERT_EQUAL_STRING("Eisen 7", clubName(Club::Iron7));
    TEST_ASSERT_EQUAL_STRING("Pt", clubShortName(Club::Putter));
    TEST_ASSERT_EQUAL_STRING("-", clubShortName(Club::None));
    TEST_ASSERT_EQUAL_STRING("-", clubName(Club::None));
    for (int i = 0; i < kClubCount; i++) {
        TEST_ASSERT_TRUE(std::strlen(clubShortName(static_cast<Club>(i))) == 2);
    }
}

static void test_default_bag_gets_shorter_club_by_club() {
    for (int i = 1; i < kClubCount; i++) TEST_ASSERT_TRUE(kBag.carry_m[i] < kBag.carry_m[i - 1]);
    TEST_ASSERT_EQUAL_INT(0, kBag.carry_m[static_cast<int>(Club::Putter)]);
}

static void test_putter_up_to_15_metres() {
    expectClub(Club::Putter, 0, false, 4);
    expectClub(Club::Putter, 3, false, 4);
    expectClub(Club::Putter, 15, false, 4);
    expectClub(Club::SandWedge, 16, false, 4);
}

static void test_putter_wins_even_as_first_shot() {
    // Nach "Vorheriges Loch" kann man am Gruen stehen und noch keinen Schlag haben.
    expectClub(Club::Putter, 5, true, 4);
}

static void test_driver_on_tee_of_par_4_and_5() {
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
    expectClub(Club::SandWedge, 70, false, 4);
    expectClub(Club::SandWedge, 30, false, 4);
    expectClub(Club::Wood3, 195, false, 5);
}

static void test_tie_goes_to_shorter_club() {
    expectClub(Club::Iron7, 145, false, 4);   // zwischen Eisen 6 (150) und Eisen 7 (140)
    expectClub(Club::Wood3, 200, false, 5);   // zwischen Driver (210) und Holz 3 (190)
}

static void test_driver_when_far_away_on_later_shots() {
    expectClub(Club::Driver, 260, false, 5);
    expectClub(Club::Driver, 9999, false, 5);
}

static void test_unknown_distance() {
    expectClub(Club::Driver, kDistanceUnknown, true, 4);
    expectClub(Club::Driver, kDistanceUnknown, true, 0);
    expectClub(Club::Iron7, kDistanceUnknown, true, 3);
    expectClub(Club::Iron7, kDistanceUnknown, false, 4);
}

static void test_unknown_par_uses_distance_on_tee() {
    expectClub(Club::Iron6, 151, true, 0);
}

static void test_custom_bag_changes_suggestion() {
    Bag bag;
    bag.carry_m[static_cast<int>(Club::Iron7)] = 120;
    bag.carry_m[static_cast<int>(Club::Iron8)] = 110;
    bag.carry_m[static_cast<int>(Club::Iron9)] = 100;
    bag.carry_m[static_cast<int>(Club::PitchingWedge)] = 90;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Iron6), static_cast<int>(suggestClub(bag, 140, false, 4)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Iron7), static_cast<int>(suggestClub(bag, 122, false, 4)));
}

static void test_suggestion_is_never_none_and_never_putter_beyond_15() {
    for (int d = -1; d <= 600; d++) {
        for (int first = 0; first < 2; first++) {
            for (int par = 0; par <= 5; par++) {
                const Club c = suggestClub(kBag, d, first != 0, par);
                TEST_ASSERT_TRUE(static_cast<int>(c) < kClubCount);
                if (d > kPuttMaxDistanceM) TEST_ASSERT_TRUE(c != Club::Putter);
            }
        }
    }
}

static void test_next_club_cycles_through_all() {
    Club c = Club::Driver;
    for (int i = 0; i < kClubCount - 1; i++) c = nextClub(c);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Putter), static_cast<int>(c));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Driver), static_cast<int>(nextClub(c)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Driver), static_cast<int>(nextClub(Club::None)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_eleven_clubs_with_names);
    RUN_TEST(test_default_bag_gets_shorter_club_by_club);
    RUN_TEST(test_putter_up_to_15_metres);
    RUN_TEST(test_putter_wins_even_as_first_shot);
    RUN_TEST(test_driver_on_tee_of_par_4_and_5);
    RUN_TEST(test_par_3_tee_shot_goes_by_distance);
    RUN_TEST(test_closest_club_on_later_shots);
    RUN_TEST(test_tie_goes_to_shorter_club);
    RUN_TEST(test_driver_when_far_away_on_later_shots);
    RUN_TEST(test_unknown_distance);
    RUN_TEST(test_unknown_par_uses_distance_on_tee);
    RUN_TEST(test_custom_bag_changes_suggestion);
    RUN_TEST(test_suggestion_is_never_none_and_never_putter_beyond_15);
    RUN_TEST(test_next_club_cycles_through_all);
    return UNITY_END();
}
