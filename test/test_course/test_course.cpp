#include <unity.h>

#include "../test_support.h"
#include "core/course.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static Hole holeWithGreen(double metres_north, float radius = kDefaultGreenRadiusM) {
    Hole h;
    h.par = 4;
    h.has_tee = true;
    h.tee = kTee;
    h.has_green = true;
    h.green_center = offset(kTee, metres_north);
    h.green_radius_m = radius;
    return h;
}

static void test_new_course_has_18_holes_without_data() {
    Course c;
    TEST_ASSERT_EQUAL_INT(18, c.hole_count);
    TEST_ASSERT_FALSE(c.holes[0].has_green);
    TEST_ASSERT_EQUAL_INT(0, c.holes[17].par);
}

static void test_distances_from_tee() {
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, kTee);
    TEST_ASSERT_TRUE(d.available);
    TEST_ASSERT_EQUAL_INT(337, d.front);
    TEST_ASSERT_EQUAL_INT(350, d.center);
    TEST_ASSERT_EQUAL_INT(363, d.back);
}

static void test_distances_shrink_while_walking() {
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, offset(kTee, 205.0));
    TEST_ASSERT_EQUAL_INT(132, d.front);
    TEST_ASSERT_EQUAL_INT(145, d.center);
    TEST_ASSERT_EQUAL_INT(158, d.back);
}

static void test_distances_from_the_side() {
    // 30 m neben der Gruenmitte: die Richtung spielt keine Rolle.
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, offset(h.green_center, 0.0, 30.0));
    TEST_ASSERT_INT_WITHIN(1, 30, d.center);
    TEST_ASSERT_INT_WITHIN(1, 17, d.front);
    TEST_ASSERT_INT_WITHIN(1, 43, d.back);
}

static void test_standing_on_green_center() {
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, h.green_center);
    TEST_ASSERT_EQUAL_INT(0, d.center);
    TEST_ASSERT_EQUAL_INT(13, d.front);
    TEST_ASSERT_EQUAL_INT(13, d.back);
}

static void test_standing_on_green_between_center_and_edge() {
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, offset(kTee, 345.0));
    TEST_ASSERT_EQUAL_INT(5, d.center);
    TEST_ASSERT_EQUAL_INT(8, d.front);
    TEST_ASSERT_EQUAL_INT(18, d.back);
}

static void test_past_the_green() {
    const Hole h = holeWithGreen(350.0);
    const GreenDistances d = greenDistances(h, offset(kTee, 390.0));
    TEST_ASSERT_EQUAL_INT(40, d.center);
    TEST_ASSERT_EQUAL_INT(27, d.front);
    TEST_ASSERT_EQUAL_INT(53, d.back);
}

static void test_custom_green_radius() {
    const Hole h = holeWithGreen(150.0, 20.0f);
    const GreenDistances d = greenDistances(h, kTee);
    TEST_ASSERT_EQUAL_INT(130, d.front);
    TEST_ASSERT_EQUAL_INT(170, d.back);
}

static void test_negative_radius_is_treated_as_zero() {
    const Hole h = holeWithGreen(150.0, -5.0f);
    const GreenDistances d = greenDistances(h, kTee);
    TEST_ASSERT_EQUAL_INT(150, d.front);
    TEST_ASSERT_EQUAL_INT(150, d.back);
}

static void test_unknown_green_gives_no_distances() {
    Hole h;
    h.par = 4;
    const GreenDistances d = greenDistances(h, kTee);
    TEST_ASSERT_FALSE(d.available);
    TEST_ASSERT_EQUAL_INT(0, d.center);
}

static void test_mark_green_makes_distances_available() {
    Hole h;
    const LatLon spot = offset(kTee, 280.0);
    markGreen(h, spot);
    TEST_ASSERT_TRUE(h.has_green);
    TEST_ASSERT_EQUAL_INT(280, greenDistances(h, kTee).center);
}

static void test_mark_green_replaces_imported_position() {
    Hole h = holeWithGreen(350.0);
    markGreen(h, offset(kTee, 360.0));
    TEST_ASSERT_EQUAL_INT(360, greenDistances(h, kTee).center);
    TEST_ASSERT_EQUAL_FLOAT(kDefaultGreenRadiusM, h.green_radius_m);
}

static void test_hole_length() {
    TEST_ASSERT_EQUAL_INT(350, holeLengthMeters(holeWithGreen(350.0)));
    Hole no_tee = holeWithGreen(350.0);
    no_tee.has_tee = false;
    TEST_ASSERT_EQUAL_INT(0, holeLengthMeters(no_tee));
    Hole empty;
    TEST_ASSERT_EQUAL_INT(0, holeLengthMeters(empty));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_new_course_has_18_holes_without_data);
    RUN_TEST(test_distances_from_tee);
    RUN_TEST(test_distances_shrink_while_walking);
    RUN_TEST(test_distances_from_the_side);
    RUN_TEST(test_standing_on_green_center);
    RUN_TEST(test_standing_on_green_between_center_and_edge);
    RUN_TEST(test_past_the_green);
    RUN_TEST(test_custom_green_radius);
    RUN_TEST(test_negative_radius_is_treated_as_zero);
    RUN_TEST(test_unknown_green_gives_no_distances);
    RUN_TEST(test_mark_green_makes_distances_available);
    RUN_TEST(test_mark_green_replaces_imported_position);
    RUN_TEST(test_hole_length);
    return UNITY_END();
}
