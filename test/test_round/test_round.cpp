#include <unity.h>

#include "../test_support.h"
#include "core/round.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

// Platz mit einem Par 4 von 350 m auf Loch 1 und einem Par 3 von 150 m auf Loch 2.
static Course makeCourse() {
    Course c;
    c.holes[0].par = 4;
    c.holes[0].has_green = true;
    c.holes[0].green_center = offset(kTee, 350.0);
    c.holes[1].par = 3;
    c.holes[1].has_green = true;
    c.holes[1].green_center = offset(kTee, 150.0);
    c.holes[2].par = 5; // Loch 3 ohne Gruen-Daten
    return c;
}

static void shot(Round& r, int hole, double metres_north, Club club) {
    TEST_ASSERT_TRUE(r.addShot(hole, offset(kTee, metres_north)));
    TEST_ASSERT_TRUE(r.setClub(hole, r.shotCount(hole) - 1, club));
}

// Loch 1 als Bogey: Driver 210 m, Eisen 7 130 m, Chip 10 m, zwei Putts.
static void playHoleOne(Round& r) {
    shot(r, 0, 0.0, Club::Driver);
    shot(r, 0, 210.0, Club::Iron7);
    shot(r, 0, 340.0, Club::SandWedge);
    shot(r, 0, 350.0, Club::Putter);
    shot(r, 0, 350.0, Club::Putter);
    TEST_ASSERT_TRUE(r.finishHole(0));
}

// ---------- Schlaege erfassen ----------

static void test_new_round_is_empty() {
    Round r;
    TEST_ASSERT_EQUAL_INT(18, r.holeCount());
    for (int h = 0; h < 18; h++) {
        TEST_ASSERT_EQUAL_INT(0, r.shotCount(h));
        TEST_ASSERT_FALSE(r.isFinished(h));
    }
}

static void test_hole_count_is_clamped() {
    TEST_ASSERT_EQUAL_INT(9, Round(9).holeCount());
    TEST_ASSERT_EQUAL_INT(1, Round(0).holeCount());
    TEST_ASSERT_EQUAL_INT(1, Round(-5).holeCount());
    TEST_ASSERT_EQUAL_INT(18, Round(99).holeCount());
}

static void test_add_shot_stores_position_without_club() {
    Round r;
    const LatLon p = offset(kTee, 100.0);
    TEST_ASSERT_TRUE(r.addShot(3, p));
    TEST_ASSERT_EQUAL_INT(1, r.shotCount(3));
    const Shot* s = r.shot(3, 0);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->has_position);
    TEST_ASSERT_EQUAL_DOUBLE(p.lat, s->position.lat);
    TEST_ASSERT_EQUAL_DOUBLE(p.lon, s->position.lon);
    TEST_ASSERT_TRUE(s->club == Club::None);
    TEST_ASSERT_EQUAL_INT(0, r.shotCount(2));
    TEST_ASSERT_EQUAL_INT(0, r.shotCount(4));
}

static void test_shot_without_gps_still_counts() {
    Round r;
    TEST_ASSERT_TRUE(r.addShotWithoutPosition(0));
    TEST_ASSERT_EQUAL_INT(1, r.shotCount(0));
    TEST_ASSERT_FALSE(r.shot(0, 0)->has_position);
}

static void test_invalid_hole_is_rejected_everywhere() {
    Round r(9);
    const int bad[] = {-1, 9, 18, 1000};
    for (int h : bad) {
        TEST_ASSERT_FALSE(r.addShot(h, kTee));
        TEST_ASSERT_FALSE(r.addShotWithoutPosition(h));
        TEST_ASSERT_FALSE(r.setClub(h, 0, Club::Driver));
        TEST_ASSERT_FALSE(r.removeLastShot(h));
        TEST_ASSERT_FALSE(r.finishHole(h));
        TEST_ASSERT_FALSE(r.isFinished(h));
        TEST_ASSERT_EQUAL_INT(0, r.shotCount(h));
        TEST_ASSERT_EQUAL_INT(0, r.putts(h));
        TEST_ASSERT_NULL(r.shot(h, 0));
    }
}

static void test_invalid_shot_index_is_rejected() {
    Round r;
    r.addShot(0, kTee);
    TEST_ASSERT_NULL(r.shot(0, -1));
    TEST_ASSERT_NULL(r.shot(0, 1));
    TEST_ASSERT_FALSE(r.setClub(0, -1, Club::Driver));
    TEST_ASSERT_FALSE(r.setClub(0, 1, Club::Driver));
}

static void test_hole_is_full_after_20_shots() {
    Round r;
    for (int i = 0; i < kMaxShotsPerHole; i++) TEST_ASSERT_TRUE(r.addShot(0, kTee));
    TEST_ASSERT_FALSE(r.addShot(0, kTee));
    TEST_ASSERT_FALSE(r.addShotWithoutPosition(0));
    TEST_ASSERT_EQUAL_INT(20, r.shotCount(0));
    TEST_ASSERT_TRUE(r.addShot(1, kTee)); // andere Loecher sind nicht betroffen
}

static void test_remove_last_shot() {
    Round r;
    shot(r, 0, 0.0, Club::Driver);
    shot(r, 0, 200.0, Club::Iron7);
    TEST_ASSERT_TRUE(r.removeLastShot(0));
    TEST_ASSERT_EQUAL_INT(1, r.shotCount(0));
    TEST_ASSERT_TRUE(r.shot(0, 0)->club == Club::Driver);
    TEST_ASSERT_TRUE(r.removeLastShot(0));
    TEST_ASSERT_FALSE(r.removeLastShot(0));
    TEST_ASSERT_EQUAL_INT(0, r.shotCount(0));
}

static void test_removed_shot_leaves_no_trace_in_next_one() {
    Round r;
    shot(r, 0, 0.0, Club::Putter);
    r.removeLastShot(0);
    r.addShotWithoutPosition(0);
    TEST_ASSERT_TRUE(r.shot(0, 0)->club == Club::None);
    TEST_ASSERT_FALSE(r.shot(0, 0)->has_position);
    TEST_ASSERT_EQUAL_INT(0, r.putts(0));
}

// ---------- Loch abschliessen ----------

static void test_cannot_finish_hole_without_shots() {
    Round r;
    TEST_ASSERT_FALSE(r.finishHole(0));
    TEST_ASSERT_FALSE(r.isFinished(0));
}

static void test_finish_hole() {
    Round r;
    playHoleOne(r);
    TEST_ASSERT_TRUE(r.isFinished(0));
    TEST_ASSERT_EQUAL_INT(5, r.shotCount(0));
    TEST_ASSERT_EQUAL_INT(2, r.putts(0));
    TEST_ASSERT_FALSE(r.isFinished(1));
}

static void test_new_shot_reopens_finished_hole() {
    Round r;
    playHoleOne(r);
    r.addShot(0, kTee);
    TEST_ASSERT_FALSE(r.isFinished(0));
    TEST_ASSERT_EQUAL_INT(6, r.shotCount(0));
}

static void test_undo_keeps_hole_finished_unless_empty() {
    Round r;
    playHoleOne(r);
    r.removeLastShot(0);
    TEST_ASSERT_TRUE(r.isFinished(0));
    TEST_ASSERT_EQUAL_INT(1, r.putts(0));
    while (r.removeLastShot(0)) {}
    TEST_ASSERT_FALSE(r.isFinished(0));
}

static void test_changing_club_updates_putts() {
    Round r;
    playHoleOne(r);
    r.setClub(0, 2, Club::Putter); // der Chip war doch ein Putt
    TEST_ASSERT_EQUAL_INT(3, r.putts(0));
}

// ---------- Schlagweiten ----------

static void test_shot_distance_to_next_shot() {
    Round r;
    const Course c = makeCourse();
    playHoleOne(r);
    ShotDistance d = shotDistance(r, c, 0, 0, nullptr);
    TEST_ASSERT_TRUE(d.available);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 210.0, d.meters);
    d = shotDistance(r, c, 0, 1, nullptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 130.0, d.meters);
    d = shotDistance(r, c, 0, 2, nullptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 10.0, d.meters);
}

static void test_putts_have_no_distance() {
    Round r;
    const Course c = makeCourse();
    playHoleOne(r);
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 3, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 4, nullptr).available);
}

static void test_last_shot_follows_player_while_hole_is_open() {
    Round r;
    const Course c = makeCourse();
    shot(r, 0, 0.0, Club::Driver);
    const LatLon walking = offset(kTee, 120.0);
    ShotDistance d = shotDistance(r, c, 0, 0, &walking);
    TEST_ASSERT_TRUE(d.available);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 120.0, d.meters);
    const LatLon at_ball = offset(kTee, 215.0);
    d = shotDistance(r, c, 0, 0, &at_ball);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 215.0, d.meters);
}

static void test_last_shot_without_player_position_has_no_distance() {
    Round r;
    const Course c = makeCourse();
    shot(r, 0, 0.0, Club::Driver);
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 0, nullptr).available);
}

static void test_holed_out_from_fairway_measures_to_green_center() {
    Round r;
    const Course c = makeCourse();
    shot(r, 1, 0.0, Club::Iron6); // Hole-in-one auf dem Par 3
    r.finishHole(1);
    const LatLon somewhere = offset(kTee, 999.0);
    const ShotDistance d = shotDistance(r, c, 1, 0, &somewhere); // Spielerposition zaehlt nicht mehr
    TEST_ASSERT_TRUE(d.available);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 150.0, d.meters);
}

static void test_finished_hole_without_green_has_no_last_distance() {
    Round r;
    const Course c = makeCourse();
    shot(r, 2, 0.0, Club::Driver);
    shot(r, 2, 200.0, Club::Iron7);
    r.finishHole(2);
    TEST_ASSERT_TRUE(shotDistance(r, c, 2, 0, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(r, c, 2, 1, nullptr).available);
}

static void test_missing_gps_positions_give_no_distance() {
    Round r;
    const Course c = makeCourse();
    r.addShotWithoutPosition(0);        // Schlag 1 ohne GPS
    r.addShot(0, offset(kTee, 200.0));  // Schlag 2 mit GPS
    r.addShotWithoutPosition(0);        // Schlag 3 ohne GPS
    r.addShot(0, offset(kTee, 340.0));  // Schlag 4 mit GPS
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 0, nullptr).available); // Start unbekannt
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 1, nullptr).available); // Ende unbekannt
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 2, nullptr).available);
}

static void test_shot_distance_with_invalid_index() {
    Round r;
    const Course c = makeCourse();
    playHoleOne(r);
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, 5, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(r, c, 0, -1, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(r, c, -1, 0, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(r, c, 18, 0, nullptr).available);
}

static void test_undo_makes_previous_shot_live_again() {
    Round r;
    const Course c = makeCourse();
    shot(r, 0, 0.0, Club::Driver);
    shot(r, 0, 210.0, Club::Iron7);
    r.removeLastShot(0);
    const LatLon player = offset(kTee, 190.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 190.0, shotDistance(r, c, 0, 0, &player).meters);
}

// ---------- Gesamtauswertung ----------

static void test_totals_of_empty_round() {
    const RoundTotals t = roundTotals(Round(), makeCourse());
    TEST_ASSERT_EQUAL_INT(0, t.holes_played);
    TEST_ASSERT_EQUAL_INT(0, t.strokes);
    TEST_ASSERT_EQUAL_INT(0, t.putts);
    TEST_ASSERT_EQUAL_INT(0, t.to_par);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, t.longest_shot_m);
}

static void test_totals_count_only_finished_holes() {
    Round r;
    const Course c = makeCourse();
    playHoleOne(r);                 // 5 Schlaege auf Par 4
    shot(r, 1, 0.0, Club::Iron6);   // Loch 2 laeuft noch
    const RoundTotals t = roundTotals(r, c);
    TEST_ASSERT_EQUAL_INT(1, t.holes_played);
    TEST_ASSERT_EQUAL_INT(5, t.strokes);
    TEST_ASSERT_EQUAL_INT(2, t.putts);
    TEST_ASSERT_EQUAL_INT(1, t.to_par);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 210.0, t.longest_shot_m);
}

static void test_totals_over_several_holes() {
    Round r;
    const Course c = makeCourse();
    playHoleOne(r);                      // +1
    shot(r, 1, 0.0, Club::Iron6);        // Birdie auf dem Par 3
    shot(r, 1, 150.0, Club::Putter);
    r.finishHole(1);
    shot(r, 2, 0.0, Club::Driver);       // Par 5 in 7, Drive 240 m
    shot(r, 2, 240.0, Club::Wood3);
    for (int i = 0; i < 5; i++) r.addShotWithoutPosition(2);
    r.finishHole(2);
    const RoundTotals t = roundTotals(r, c);
    TEST_ASSERT_EQUAL_INT(3, t.holes_played);
    TEST_ASSERT_EQUAL_INT(14, t.strokes);
    TEST_ASSERT_EQUAL_INT(3, t.putts);
    TEST_ASSERT_EQUAL_INT(2, t.to_par); // +1 -1 +2
    TEST_ASSERT_EQUAL_INT(3, t.holes_with_par);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 240.0, t.longest_shot_m);
}

static void test_holes_without_par_do_not_distort_to_par() {
    Round r;
    Course c = makeCourse();
    c.holes[5].par = 0;
    playHoleOne(r);
    for (int i = 0; i < 6; i++) r.addShotWithoutPosition(5);
    r.finishHole(5);
    const RoundTotals t = roundTotals(r, c);
    TEST_ASSERT_EQUAL_INT(2, t.holes_played);
    TEST_ASSERT_EQUAL_INT(11, t.strokes);
    TEST_ASSERT_EQUAL_INT(1, t.holes_with_par);
    TEST_ASSERT_EQUAL_INT(1, t.to_par);
}

static void test_totals_respect_nine_hole_round() {
    Round r(9);
    const Course c = makeCourse();
    playHoleOne(r);
    TEST_ASSERT_EQUAL_INT(1, roundTotals(r, c).holes_played);
}

static void test_full_round_of_pars() {
    Round r;
    Course c;
    int par_sum = 0;
    for (int h = 0; h < 18; h++) {
        c.holes[h].par = 3 + h % 3;
        par_sum += c.holes[h].par;
        for (int i = 0; i < c.holes[h].par; i++) r.addShotWithoutPosition(h);
        r.setClub(h, c.holes[h].par - 1, Club::Putter);
        r.setClub(h, c.holes[h].par - 2, Club::Putter);
        r.finishHole(h);
    }
    const RoundTotals t = roundTotals(r, c);
    TEST_ASSERT_EQUAL_INT(18, t.holes_played);
    TEST_ASSERT_EQUAL_INT(par_sum, t.strokes);
    TEST_ASSERT_EQUAL_INT(72, par_sum);
    TEST_ASSERT_EQUAL_INT(0, t.to_par);
    TEST_ASSERT_EQUAL_INT(36, t.putts);
}

// ---------- Bezeichnung des Ergebnisses ----------

static void expectName(ScoreName expected, int strokes, int par) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(scoreName(strokes, par)));
}

static void test_score_names() {
    expectName(ScoreName::HoleInOne, 1, 3);
    expectName(ScoreName::HoleInOne, 1, 4);
    expectName(ScoreName::Albatross, 2, 5);
    expectName(ScoreName::Eagle, 2, 4);
    expectName(ScoreName::Eagle, 3, 5);
    expectName(ScoreName::Birdie, 2, 3);
    expectName(ScoreName::Birdie, 4, 5);
    expectName(ScoreName::Par, 3, 3);
    expectName(ScoreName::Par, 5, 5);
    expectName(ScoreName::Bogey, 5, 4);
    expectName(ScoreName::DoubleBogey, 6, 4);
    expectName(ScoreName::Worse, 7, 4);
    expectName(ScoreName::Worse, 20, 3);
}

static void test_score_name_unknown_without_par_or_strokes() {
    expectName(ScoreName::Unknown, 4, 0);
    expectName(ScoreName::Unknown, 0, 4);
    expectName(ScoreName::Unknown, -1, 4);
    expectName(ScoreName::Unknown, 4, -1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_new_round_is_empty);
    RUN_TEST(test_hole_count_is_clamped);
    RUN_TEST(test_add_shot_stores_position_without_club);
    RUN_TEST(test_shot_without_gps_still_counts);
    RUN_TEST(test_invalid_hole_is_rejected_everywhere);
    RUN_TEST(test_invalid_shot_index_is_rejected);
    RUN_TEST(test_hole_is_full_after_20_shots);
    RUN_TEST(test_remove_last_shot);
    RUN_TEST(test_removed_shot_leaves_no_trace_in_next_one);
    RUN_TEST(test_cannot_finish_hole_without_shots);
    RUN_TEST(test_finish_hole);
    RUN_TEST(test_new_shot_reopens_finished_hole);
    RUN_TEST(test_undo_keeps_hole_finished_unless_empty);
    RUN_TEST(test_changing_club_updates_putts);
    RUN_TEST(test_shot_distance_to_next_shot);
    RUN_TEST(test_putts_have_no_distance);
    RUN_TEST(test_last_shot_follows_player_while_hole_is_open);
    RUN_TEST(test_last_shot_without_player_position_has_no_distance);
    RUN_TEST(test_holed_out_from_fairway_measures_to_green_center);
    RUN_TEST(test_finished_hole_without_green_has_no_last_distance);
    RUN_TEST(test_missing_gps_positions_give_no_distance);
    RUN_TEST(test_shot_distance_with_invalid_index);
    RUN_TEST(test_undo_makes_previous_shot_live_again);
    RUN_TEST(test_totals_of_empty_round);
    RUN_TEST(test_totals_count_only_finished_holes);
    RUN_TEST(test_totals_over_several_holes);
    RUN_TEST(test_holes_without_par_do_not_distort_to_par);
    RUN_TEST(test_totals_respect_nine_hole_round);
    RUN_TEST(test_full_round_of_pars);
    RUN_TEST(test_score_names);
    RUN_TEST(test_score_name_unknown_without_par_or_strokes);
    return UNITY_END();
}
