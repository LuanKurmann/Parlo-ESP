#include <cstring>
#include <string>

#include <unity.h>

#include "../test_support.h"
#include "core/serialize.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static Course sampleCourse() {
    Course c;
    std::strcpy(c.name, "Golfclub Beispiel");
    c.hole_count = 9;
    for (int h = 0; h < 9; h++) c.holes[h].par = 3 + h % 3;
    c.holes[0].has_green = true;
    c.holes[0].green_center = LatLon{47.3800123, 8.5456789};
    c.holes[0].green_radius_m = 17.5f;
    c.holes[0].has_tee = true;
    c.holes[0].tee = LatLon{47.3769001, 8.5417002};
    c.holes[1].has_green = true;
    c.holes[1].green_center = LatLon{-33.8915004, -151.2020005}; // Sueden und Westen
    c.holes[8].par = 0; // unbekanntes Par
    return c;
}

static void assertSameCourse(const Course& a, const Course& b) {
    TEST_ASSERT_EQUAL_STRING(a.name, b.name);
    TEST_ASSERT_EQUAL_INT(a.hole_count, b.hole_count);
    for (int h = 0; h < a.hole_count; h++) {
        TEST_ASSERT_EQUAL_INT(a.holes[h].par, b.holes[h].par);
        TEST_ASSERT_EQUAL_INT(a.holes[h].has_green, b.holes[h].has_green);
        TEST_ASSERT_EQUAL_INT(a.holes[h].has_tee, b.holes[h].has_tee);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, a.holes[h].green_radius_m, b.holes[h].green_radius_m);
        if (a.holes[h].has_green) {
            TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, a.holes[h].green_center.lat, b.holes[h].green_center.lat);
            TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, a.holes[h].green_center.lon, b.holes[h].green_center.lon);
        }
        if (a.holes[h].has_tee) {
            TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, a.holes[h].tee.lat, b.holes[h].tee.lat);
            TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, a.holes[h].tee.lon, b.holes[h].tee.lon);
        }
    }
}

static Round sampleRound() {
    Round r(9);
    r.addShot(0, LatLon{47.3769001, 8.5417002});
    r.setClub(0, 0, Club::Driver);
    r.addShotWithoutPosition(0);
    r.setClub(0, 1, Club::Iron7);
    r.addShot(0, LatLon{47.3800123, 8.5456789});
    r.setClub(0, 2, Club::Putter);
    r.finishHole(0);
    r.addShot(1, LatLon{-33.8915004, -151.2020005});
    r.setClub(1, 0, Club::PitchingWedge);
    r.addShotWithoutPosition(1); // Schlaeger noch nicht gewaehlt
    return r;
}

static void assertSameRound(const Round& a, const Round& b) {
    TEST_ASSERT_EQUAL_INT(a.holeCount(), b.holeCount());
    for (int h = 0; h < a.holeCount(); h++) {
        TEST_ASSERT_EQUAL_INT(a.shotCount(h), b.shotCount(h));
        TEST_ASSERT_EQUAL_INT(a.isFinished(h), b.isFinished(h));
        for (int i = 0; i < a.shotCount(h); i++) {
            const Shot* x = a.shot(h, i);
            const Shot* y = b.shot(h, i);
            TEST_ASSERT_EQUAL_INT(static_cast<int>(x->club), static_cast<int>(y->club));
            TEST_ASSERT_EQUAL_INT(x->has_position, y->has_position);
            if (x->has_position) {
                TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, x->position.lat, y->position.lat);
                TEST_ASSERT_DOUBLE_WITHIN(0.5e-7, x->position.lon, y->position.lon);
            }
        }
    }
}

// ---------- Platz ----------

static void test_course_round_trip() {
    const Course original = sampleCourse();
    Course loaded;
    TEST_ASSERT_TRUE(courseFromJson(courseToJson(original), loaded));
    assertSameCourse(original, loaded);
}

static void test_empty_default_course_round_trip() {
    Course loaded;
    loaded.hole_count = 3;
    TEST_ASSERT_TRUE(courseFromJson(courseToJson(Course()), loaded));
    TEST_ASSERT_EQUAL_INT(18, loaded.hole_count);
    TEST_ASSERT_FALSE(loaded.holes[0].has_green);
    TEST_ASSERT_EQUAL_STRING("", loaded.name);
}

static void test_course_json_is_stable() {
    // Zweimal umwandeln ergibt denselben Text (wichtig fuer "nur speichern, wenn geaendert").
    const std::string once = courseToJson(sampleCourse());
    Course loaded;
    courseFromJson(once, loaded);
    TEST_ASSERT_TRUE(once == courseToJson(loaded));
}

static void test_course_from_handwritten_json() {
    // So koennte der Import vom Handy aussehen.
    const char* json = R"({"v":1,"name":"Testplatz","holes":[
        {"par":4,"green":[473800000,85400000],"r":150,"tee":[473770000,85410000]},
        {"par":3},
        {"par":5,"green":[473900000,85500000]}]})";
    Course c;
    TEST_ASSERT_TRUE(courseFromJson(json, c));
    TEST_ASSERT_EQUAL_STRING("Testplatz", c.name);
    TEST_ASSERT_EQUAL_INT(3, c.hole_count);
    TEST_ASSERT_TRUE(c.holes[0].has_green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, c.holes[0].green_radius_m);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 47.38, c.holes[0].green_center.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 8.541, c.holes[0].tee.lon);
    TEST_ASSERT_FALSE(c.holes[1].has_green);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kDefaultGreenRadiusM, c.holes[2].green_radius_m);
    TEST_ASSERT_FALSE(c.holes[2].has_tee);
}

static void test_course_name_with_umlauts_and_quotes() {
    Course c;
    std::strcpy(c.name, "Z\xc3\xbcrich \"Nord\" \\ 1");
    Course loaded;
    TEST_ASSERT_TRUE(courseFromJson(courseToJson(c), loaded));
    TEST_ASSERT_EQUAL_STRING(c.name, loaded.name);
}

static void test_invalid_course_json_is_rejected_and_leaves_target_untouched() {
    const char* bad[] = {
        "",
        "kein json",
        "[]",
        "42",
        "null",
        R"({"v":1})",                                                  // keine Loecher
        R"({"v":2,"holes":[{"par":4}]})",                              // andere Version
        R"({"holes":[{"par":4}]})",                                    // Version fehlt
        R"({"v":"1","holes":[{"par":4}]})",
        R"({"v":1,"holes":[]})",                                       // 0 Loecher
        R"({"v":1,"holes":{}})",
        R"({"v":1,"holes":[4]})",
        R"({"v":1,"holes":[{}]})",                                     // Par fehlt
        R"({"v":1,"holes":[{"par":"4"}]})",
        R"({"v":1,"holes":[{"par":7}]})",
        R"({"v":1,"holes":[{"par":2}]})",
        R"({"v":1,"holes":[{"par":-1}]})",
        R"({"v":1,"holes":[{"par":4.5}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000]}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000,80000000,1]}]})",
        R"({"v":1,"holes":[{"par":4,"green":"47,8"}]})",
        R"({"v":1,"holes":[{"par":4,"green":[910000000,80000000]}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000,1810000000]}]})",
        R"({"v":1,"holes":[{"par":4,"green":[-905000000,80000000]}]})",
        R"({"v":1,"holes":[{"par":4,"green":["470000000",80000000]}]})",
        R"({"v":1,"holes":[{"par":4,"tee":[470000000,-2000000000]}]})",
        R"({"v":1,"holes":[{"par":4,"green":[47.38,8.54]}]})",              // Grad statt Zehnmillionstel
        R"({"v":1,"holes":[{"par":4,"green":[99999999999,80000000]}]})",    // sprengt 32 Bit
        R"({"v":1,"holes":[{"par":4,"green":[470000000,80000000],"r":13.5}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000,80000000],"r":9}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000,80000000],"r":601}]})",
        R"({"v":1,"holes":[{"par":4,"green":[470000000,80000000],"r":"gross"}]})",
        R"({"v":1,"name":42,"holes":[{"par":4}]})",
        R"({"v":1,"name":"Dieser Platzname ist mit Absicht viel zu lang geraten","holes":[{"par":4}]})",
        R"({"v":1,"name":"Zeile\numbruch","holes":[{"par":4}]})",
        R"({"v":1,"holes":[{"par":4})",                                // abgeschnitten
    };
    const Course before = sampleCourse();
    for (const char* json : bad) {
        Course c = before;
        TEST_ASSERT_FALSE_MESSAGE(courseFromJson(json, c), json);
        assertSameCourse(before, c);
    }
}

static void test_course_with_19_holes_is_rejected() {
    std::string json = R"({"v":1,"holes":[)";
    for (int i = 0; i < 19; i++) json += std::string(i ? "," : "") + R"({"par":4})";
    json += "]}";
    Course c;
    TEST_ASSERT_FALSE(courseFromJson(json, c));
}

// ---------- Runde ----------

static void test_round_round_trip() {
    const Round original = sampleRound();
    DateTime started;
    started.valid = true;
    started.year = 2026;
    started.month = 11;
    started.day = 23;
    started.hour = 8;
    started.minute = 5;
    RoundState loaded;
    TEST_ASSERT_TRUE(roundFromJson(roundToJson(original, 1, started, "Golfclub Beispiel"), loaded));
    assertSameRound(original, loaded.round);
    TEST_ASSERT_EQUAL_INT(1, loaded.hole);
    TEST_ASSERT_EQUAL_STRING("Golfclub Beispiel", loaded.course_name);
    TEST_ASSERT_TRUE(loaded.started.valid);
    TEST_ASSERT_EQUAL_INT(2026, loaded.started.year);
    TEST_ASSERT_EQUAL_INT(11, loaded.started.month);
    TEST_ASSERT_EQUAL_INT(23, loaded.started.day);
    TEST_ASSERT_EQUAL_INT(8, loaded.started.hour);
    TEST_ASSERT_EQUAL_INT(5, loaded.started.minute);
}

static void test_empty_round_round_trip_without_date() {
    RoundState loaded;
    loaded.hole = 5;
    TEST_ASSERT_TRUE(roundFromJson(roundToJson(Round(), 0, DateTime(), ""), loaded));
    TEST_ASSERT_EQUAL_INT(18, loaded.round.holeCount());
    TEST_ASSERT_EQUAL_INT(0, loaded.hole);
    TEST_ASSERT_FALSE(loaded.started.valid);
    TEST_ASSERT_EQUAL_INT(0, loaded.round.shotCount(0));
}

static void test_null_course_name_is_written_as_empty() {
    RoundState loaded;
    TEST_ASSERT_TRUE(roundFromJson(roundToJson(Round(), 0, DateTime(), nullptr), loaded));
    TEST_ASSERT_EQUAL_STRING("", loaded.course_name);
}

static void test_round_json_is_stable() {
    const std::string once = roundToJson(sampleRound(), 1, DateTime(), "X");
    RoundState loaded;
    roundFromJson(once, loaded);
    TEST_ASSERT_TRUE(once == roundToJson(loaded.round, loaded.hole, loaded.started, loaded.course_name));
}

static void test_fullest_possible_round_round_trips() {
    // 18 Loecher mit je 20 Schlaegen: groesste Datei, die entstehen kann.
    Round r;
    for (int h = 0; h < 18; h++) {
        for (int i = 0; i < kMaxShotsPerHole; i++) {
            r.addShot(h, offset(kTee, h * 300.0 + i * 15.0, i * 3.0));
            r.setClub(h, i, static_cast<Club>((h + i) % kClubCount));
        }
        r.finishHole(h);
    }
    const std::string json = roundToJson(r, 17, DateTime(), "Voll");
    TEST_ASSERT_TRUE(json.size() < 40000); // passt bequem in den Speicher
    RoundState loaded;
    TEST_ASSERT_TRUE(roundFromJson(json, loaded));
    assertSameRound(r, loaded.round);
}

static void test_positions_keep_centimetre_precision() {
    Round r;
    const LatLon p{47.123456789, -8.987654321};
    r.addShot(0, p);
    RoundState loaded;
    roundFromJson(roundToJson(r, 0, DateTime(), ""), loaded);
    TEST_ASSERT_TRUE(distanceMeters(p, loaded.round.shot(0, 0)->position) < 0.01);
}

static void test_invalid_round_json_is_rejected_and_leaves_target_untouched() {
    const char* bad[] = {
        "",
        "{}",
        R"({"v":1,"hole":0})",                                                        // keine Loecher
        R"({"v":3,"hole":0,"holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"holes":[{"done":false,"shots":[]}]})",                             // Loch fehlt
        R"({"v":1,"hole":1,"holes":[{"done":false,"shots":[]}]})",                    // Loch ausserhalb
        R"({"v":1,"hole":-1,"holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":"0","holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"holes":[]})",
        R"({"v":1,"hole":0,"holes":[{"shots":[]}]})",                                 // done fehlt
        R"({"v":1,"hole":0,"holes":[{"done":1,"shots":[]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false}]})",                               // shots fehlt
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":{}}]})",
        R"({"v":1,"hole":0,"holes":[{"done":true,"shots":[]}]})",                     // fertig ohne Schlag
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[5]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"c":"XX"}]}]})",          // unbekannter Schlaeger
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"c":"-"}]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"c":3}]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"p":[950000000,80000000]}]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"p":[470000000]}]}]})",
        R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[{"p":"hier"}]}]})",
        R"({"v":1,"hole":0,"date":"gestern","holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"date":"2026-13-01T08:00","holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"date":"2026-11-23T24:00","holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"date":"2026-11-23T08:00:00","holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"date":20261123,"holes":[{"done":false,"shots":[]}]})",
        R"({"v":1,"hole":0,"course":7,"holes":[{"done":false,"shots":[]}]})",
    };
    for (const char* json : bad) {
        RoundState state;
        state.hole = 3;
        state.round.addShotWithoutPosition(3);
        TEST_ASSERT_FALSE_MESSAGE(roundFromJson(json, state), json);
        TEST_ASSERT_EQUAL_INT(3, state.hole);
        TEST_ASSERT_EQUAL_INT(1, state.round.shotCount(3));
    }
}

static void test_round_with_too_many_shots_or_holes_is_rejected() {
    std::string shots;
    for (int i = 0; i < kMaxShotsPerHole + 1; i++) shots += std::string(i ? "," : "") + R"({"c":"Pt"})";
    RoundState state;
    TEST_ASSERT_FALSE(roundFromJson(R"({"v":1,"hole":0,"holes":[{"done":false,"shots":[)" + shots + "]}]}", state));

    std::string holes;
    for (int i = 0; i < 19; i++) holes += std::string(i ? "," : "") + R"({"done":false,"shots":[]})";
    TEST_ASSERT_FALSE(roundFromJson(R"({"v":1,"hole":0,"holes":[)" + holes + "]}", state));
}

static void test_all_club_names_survive() {
    Round r(1);
    for (int i = 0; i < kClubCount; i++) {
        r.addShotWithoutPosition(0);
        r.setClub(0, i, static_cast<Club>(i));
    }
    RoundState loaded;
    TEST_ASSERT_TRUE(roundFromJson(roundToJson(r, 0, DateTime(), ""), loaded));
    assertSameRound(r, loaded.round);
}

// ---------- Beschaedigte Texte ----------

static void test_damaged_json_never_crashes_and_never_yields_invalid_data() {
    // Jedes einzelne Zeichen der Reihe nach durch andere ersetzen bzw. entfernen.
    const std::string round_json = roundToJson(sampleRound(), 1, DateTime(), "X");
    const std::string course_json = courseToJson(sampleCourse());
    const char replacements[] = {'0', '9', '"', '{', ']', ',', '-', 'x', ' '};
    for (const std::string* source : {&round_json, &course_json}) {
        for (size_t i = 0; i < source->size(); i++) {
            for (int variant = 0; variant <= static_cast<int>(sizeof(replacements)); variant++) {
                std::string damaged = *source;
                if (variant == static_cast<int>(sizeof(replacements))) {
                    damaged.erase(i, 1);
                } else {
                    damaged[i] = replacements[variant];
                }
                RoundState state;
                if (roundFromJson(damaged, state)) {
                    TEST_ASSERT_TRUE(state.hole >= 0 && state.hole < state.round.holeCount());
                    for (int h = 0; h < state.round.holeCount(); h++) {
                        for (int s = 0; s < state.round.shotCount(h); s++) {
                            const Shot* shot = state.round.shot(h, s);
                            if (shot->has_position) {
                                TEST_ASSERT_TRUE(shot->position.lat >= -90.0 && shot->position.lat <= 90.0);
                                TEST_ASSERT_TRUE(shot->position.lon >= -180.0 && shot->position.lon <= 180.0);
                            }
                        }
                    }
                }
                Course course;
                if (courseFromJson(damaged, course)) {
                    TEST_ASSERT_TRUE(course.hole_count >= 1 && course.hole_count <= kMaxHoles);
                    for (int h = 0; h < course.hole_count; h++) {
                        TEST_ASSERT_TRUE(course.holes[h].par >= 0 && course.holes[h].par <= 6);
                        TEST_ASSERT_TRUE(course.holes[h].green_radius_m >= 1.0f && course.holes[h].green_radius_m <= 60.0f);
                    }
                }
            }
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_course_round_trip);
    RUN_TEST(test_empty_default_course_round_trip);
    RUN_TEST(test_course_json_is_stable);
    RUN_TEST(test_course_from_handwritten_json);
    RUN_TEST(test_course_name_with_umlauts_and_quotes);
    RUN_TEST(test_invalid_course_json_is_rejected_and_leaves_target_untouched);
    RUN_TEST(test_course_with_19_holes_is_rejected);
    RUN_TEST(test_round_round_trip);
    RUN_TEST(test_empty_round_round_trip_without_date);
    RUN_TEST(test_null_course_name_is_written_as_empty);
    RUN_TEST(test_round_json_is_stable);
    RUN_TEST(test_fullest_possible_round_round_trips);
    RUN_TEST(test_positions_keep_centimetre_precision);
    RUN_TEST(test_invalid_round_json_is_rejected_and_leaves_target_untouched);
    RUN_TEST(test_round_with_too_many_shots_or_holes_is_rejected);
    RUN_TEST(test_all_club_names_survive);
    RUN_TEST(test_damaged_json_never_crashes_and_never_yields_invalid_data);
    return UNITY_END();
}
