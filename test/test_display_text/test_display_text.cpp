#include <unity.h>

#include "../test_support.h"
#include "core/display_text.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static Course makeCourse() {
    Course c;
    c.holes[0].par = 5;
    c.holes[0].has_tee = true;
    c.holes[0].tee = kTee;
    c.holes[0].has_green = true;
    c.holes[0].green_center = offset(kTee, 487.0);
    c.holes[1].par = 3; // Gruen und Abschlag unbekannt
    c.holes[2].par = 0;
    c.holes[2].has_green = true; // Gruen selbst markiert, kein Abschlag, kein Par
    c.holes[2].green_center = offset(kTee, 300.0);
    return c;
}

static void hit(App& app, double metres_north) {
    app.setPosition(offset(kTee, metres_north));
    app.button1Short();
    app.button1Short();
}

static void nextHole(App& app) {
    hit(app, 0.0);
    app.button2Short();
    app.button2Short();
}

static void test_tee_of_first_hole() {
    App app(makeCourse());
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 1", t.title);
    TEST_ASSERT_EQUAL_STRING("Par 5 · 487 m", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("500", t.back);
    TEST_ASSERT_EQUAL_STRING("487", t.center);
    TEST_ASSERT_EQUAL_STRING("474", t.front);
    TEST_ASSERT_EQUAL_STRING("Noch kein Schlag", t.foot);
    TEST_ASSERT_EQUAL_STRING("", t.hint);
    TEST_ASSERT_TRUE(t.gps_ok);
}

static void test_as_in_the_prototype_after_the_drive() {
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, 218.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("282", t.back);
    TEST_ASSERT_EQUAL_STRING("269", t.center);
    TEST_ASSERT_EQUAL_STRING("256", t.front);
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 218 m", t.foot);
}

static void test_plural_and_putt_without_distance() {
    App app(makeCourse());
    hit(app, 0.0);
    hit(app, 218.0);
    app.setPosition(offset(kTee, 380.0));
    TEST_ASSERT_EQUAL_STRING("2 Schläge · letzter 162 m", distanceScreenText(app).foot);
    hit(app, 485.0); // Putter vorgeschlagen und bestaetigt
    TEST_ASSERT_EQUAL_STRING("3 Schläge", distanceScreenText(app).foot);
}

static void test_no_gps() {
    App app(makeCourse());
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("---", t.center);
    TEST_ASSERT_EQUAL_STRING("---", t.front);
    TEST_ASSERT_EQUAL_STRING("---", t.back);
    TEST_ASSERT_EQUAL_STRING("Kein GPS-Empfang", t.hint);
    TEST_ASSERT_FALSE(t.gps_ok);
    TEST_ASSERT_EQUAL_STRING("Par 5 · 487 m", t.subtitle); // Lochdaten bleiben sichtbar
}

static void test_shot_counted_without_gps_shows_no_distance() {
    App app(makeCourse());
    app.button1Short();
    app.button1Short();
    TEST_ASSERT_EQUAL_STRING("1 Schlag", distanceScreenText(app).foot);
}

static void test_hole_without_green() {
    App app(makeCourse());
    nextHole(app);
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 2", t.title);
    TEST_ASSERT_EQUAL_STRING("Par 3", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("---", t.center);
    TEST_ASSERT_EQUAL_STRING("Grün nicht markiert", t.hint);
    TEST_ASSERT_TRUE(t.gps_ok);
}

static void test_no_gps_wins_over_missing_green() {
    App app(makeCourse());
    nextHole(app);
    app.clearPosition();
    TEST_ASSERT_EQUAL_STRING("Kein GPS-Empfang", distanceScreenText(app).hint);
}

static void test_hole_without_par_and_tee() {
    App app(makeCourse());
    nextHole(app);
    nextHole(app);
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 3", t.title);
    TEST_ASSERT_EQUAL_STRING("", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("300", t.center);
}

static void test_length_without_par() {
    Course c = makeCourse();
    c.holes[0].par = 0;
    App app(c);
    TEST_ASSERT_EQUAL_STRING("487 m", distanceScreenText(app).subtitle);
}

static void test_hole_18_title_fits() {
    Course c = makeCourse();
    App app(c);
    for (int i = 0; i < 17; i++) nextHole(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 18", distanceScreenText(app).title);
}

static void test_far_away_is_capped_at_999() {
    // Auf dem Parkplatz oder zu Hause: nie mehr als drei Ziffern.
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, -25000.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("999", t.back);
    TEST_ASSERT_EQUAL_STRING("999", t.center);
    TEST_ASSERT_EQUAL_STRING("999", t.front);
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 999 m", t.foot);
}

static void test_on_the_green() {
    App app(makeCourse());
    app.setPosition(offset(kTee, 487.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("0", t.center);
    TEST_ASSERT_EQUAL_STRING("13", t.front);
    TEST_ASSERT_EQUAL_STRING("13", t.back);
}

static void test_twenty_shots_fit() {
    App app(makeCourse());
    for (int i = 0; i < kMaxShotsPerHole; i++) hit(app, i * 10.0);
    app.setPosition(offset(kTee, 400.0));
    TEST_ASSERT_EQUAL_STRING("20 Schläge · letzter 210 m", distanceScreenText(app).foot);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tee_of_first_hole);
    RUN_TEST(test_as_in_the_prototype_after_the_drive);
    RUN_TEST(test_plural_and_putt_without_distance);
    RUN_TEST(test_no_gps);
    RUN_TEST(test_shot_counted_without_gps_shows_no_distance);
    RUN_TEST(test_hole_without_green);
    RUN_TEST(test_no_gps_wins_over_missing_green);
    RUN_TEST(test_hole_without_par_and_tee);
    RUN_TEST(test_length_without_par);
    RUN_TEST(test_hole_18_title_fits);
    RUN_TEST(test_far_away_is_capped_at_999);
    RUN_TEST(test_on_the_green);
    RUN_TEST(test_twenty_shots_fit);
    return UNITY_END();
}
