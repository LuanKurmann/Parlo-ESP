// Referenzwerte: geodaetische Distanz auf WGS84, berechnet mit GeographicLib.

#include <cmath>

#include <unity.h>

#include "core/geo.h"

using golf::distanceMeters;
using golf::LatLon;

void setUp() {}
void tearDown() {}

static void expectDistance(double expected, double tolerance, LatLon a, LatLon b) {
    TEST_ASSERT_DOUBLE_WITHIN(tolerance, expected, distanceMeters(a, b));
}

static void test_same_point_is_zero() {
    expectDistance(0.0, 1e-9, {47.3769, 8.5417}, {47.3769, 8.5417});
}

static void test_short_putt() {
    expectDistance(3.3340, 0.001, {47.3769, 8.5417}, {47.376922, 8.541730});
}

static void test_iron_shot_switzerland() {
    expectDistance(141.8544, 0.01, {47.3769, 8.5417}, {47.37812, 8.54225});
}

static void test_par5_length_switzerland() {
    expectDistance(496.9236, 0.01, {47.3769, 8.5417}, {47.3805, 8.5456});
}

static void test_across_equator() {
    expectDistance(181.8106, 0.01, {0.0005, -78.4500}, {-0.0008, -78.4490});
}

static void test_scotland_west_longitude() {
    expectDistance(318.6204, 0.01, {56.3431, -2.8025}, {56.3452, -2.8060});
}

static void test_southern_hemisphere() {
    expectDistance(248.8165, 0.01, {-33.8900, 151.2000}, {-33.8915, 151.2020});
}

static void test_across_date_line() {
    expectDistance(174.0187, 0.01, {-16.5000, 179.9992}, {-16.5003, -179.9992});
}

static void test_far_north() {
    expectDistance(407.7589, 0.01, {69.6500, 18.9500}, {69.6530, 18.9560});
}

static void test_five_kilometres_within_ten_centimetres() {
    expectDistance(5180.6433, 0.10, {47.3769, 8.5417}, {47.4100, 8.5900});
}

static void test_is_symmetric() {
    const LatLon a{47.3769, 8.5417}, b{47.3805, 8.5456};
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, distanceMeters(a, b), distanceMeters(b, a));
}

static void test_one_metre_steps_add_up() {
    // 200 Schritte von rund einem Meter nach Norden muessen die Gesamtstrecke ergeben.
    const double step = 0.000009;
    double sum = 0.0;
    for (int i = 0; i < 200; i++) {
        sum += distanceMeters({47.0 + i * step, 8.0}, {47.0 + (i + 1) * step, 8.0});
    }
    TEST_ASSERT_DOUBLE_WITHIN(0.001, distanceMeters({47.0, 8.0}, {47.0 + 200 * step, 8.0}), sum);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_same_point_is_zero);
    RUN_TEST(test_short_putt);
    RUN_TEST(test_iron_shot_switzerland);
    RUN_TEST(test_par5_length_switzerland);
    RUN_TEST(test_across_equator);
    RUN_TEST(test_scotland_west_longitude);
    RUN_TEST(test_southern_hemisphere);
    RUN_TEST(test_across_date_line);
    RUN_TEST(test_far_north);
    RUN_TEST(test_five_kilometres_within_ten_centimetres);
    RUN_TEST(test_is_symmetric);
    RUN_TEST(test_one_metre_steps_add_up);
    return UNITY_END();
}
