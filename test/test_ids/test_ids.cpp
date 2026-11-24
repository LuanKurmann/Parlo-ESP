#include <cstring>
#include <set>
#include <string>

#include <unity.h>

#include "core/id.h"
#include "core/serialize.h"

using namespace golf;

void setUp() {}
void tearDown() {}

static uint32_t counter_value = 0;
static uint32_t countingRandom() { return counter_value += 0x01234567u; }

static void test_valid_ids() {
    TEST_ASSERT_TRUE(isValidId("0123456789abcdef"));
    TEST_ASSERT_TRUE(isValidId("0000000000000000"));
    TEST_ASSERT_TRUE(isValidId("ffffffffffffffff"));
}

static void test_invalid_ids() {
    const char* bad[] = {"", "0123456789abcde", "0123456789abcdef0", "0123456789ABCDEF", "0123456789abcdeg",
                         "0123 56789abcdef", "0123456789abcde\n", "../../etc/passwd"};
    for (const char* id : bad) TEST_ASSERT_FALSE_MESSAGE(isValidId(id), id);
    TEST_ASSERT_FALSE(isValidId(nullptr));
}

static void test_make_id_uses_both_random_words() {
    counter_value = 0;
    Id a, b;
    makeId(a, countingRandom);
    makeId(b, countingRandom);
    TEST_ASSERT_EQUAL_STRING("0123456702468ace", a);
    TEST_ASSERT_EQUAL_STRING("0369d035048d159c", b); // dritter und vierter Aufruf
    TEST_ASSERT_TRUE(isValidId(a) && isValidId(b));
}

static void test_make_id_without_random_source_is_still_valid_and_distinct() {
    std::set<std::string> seen;
    for (uint32_t seed = 0; seed < 50; seed++) {
        for (int repeat = 0; repeat < 20; repeat++) { // gleicher Ausgangswert, trotzdem verschieden
            Id id;
            makeId(id, nullptr, seed);
            TEST_ASSERT_TRUE(isValidId(id));
            TEST_ASSERT_TRUE_MESSAGE(seen.insert(id).second, id);
        }
    }
}

static void test_id_from_content_is_stable_and_content_dependent() {
    Id a, b, c, d;
    idFromContent(a, "{\"v\":1,\"hole\":0}");
    idFromContent(b, "{\"v\":1,\"hole\":0}");
    idFromContent(c, "{\"v\":1,\"hole\":1}");
    idFromContent(d, "");
    TEST_ASSERT_EQUAL_STRING(a, b);
    TEST_ASSERT_TRUE(std::strcmp(a, c) != 0);
    TEST_ASSERT_TRUE(isValidId(a) && isValidId(c) && isValidId(d));
}

static void test_id_from_content_rarely_collides() {
    std::set<std::string> seen;
    for (int i = 0; i < 20000; i++) {
        Id id;
        idFromContent(id, "{\"v\":1,\"hole\":0,\"n\":" + std::to_string(i) + "}");
        TEST_ASSERT_TRUE(seen.insert(id).second);
    }
}

// ---------- Kennungen in den Dateien ----------

static void test_course_id_and_revision_round_trip() {
    Course course;
    std::strcpy(course.id, "00aa11bb22cc33dd");
    course.revision = 42;
    std::strcpy(course.name, "Mit Kennung");
    Course loaded;
    TEST_ASSERT_TRUE(courseFromJson(courseToJson(course), loaded));
    TEST_ASSERT_EQUAL_STRING("00aa11bb22cc33dd", loaded.id);
    TEST_ASSERT_EQUAL_UINT32(42, loaded.revision);
}

static void test_course_without_id_writes_neither_id_nor_revision() {
    // Dateien frueherer Fassungen bleiben lesbar, und ohne Kennung aendert sich am Format nichts.
    const std::string json = courseToJson(Course());
    TEST_ASSERT_TRUE(json.find("\"id\"") == std::string::npos);
    TEST_ASSERT_TRUE(json.find("\"rev\"") == std::string::npos);
    Course loaded;
    std::strcpy(loaded.id, "ffffffffffffffff");
    TEST_ASSERT_TRUE(courseFromJson(json, loaded));
    TEST_ASSERT_EQUAL_STRING("", loaded.id);
    TEST_ASSERT_EQUAL_UINT32(0, loaded.revision);
}

static void test_round_ids_round_trip() {
    Course course;
    std::strcpy(course.id, "00aa11bb22cc33dd");
    Round round;
    round.addShotWithoutPosition(0);
    const std::string json = roundToJson(round, 0, DateTime(), "X", &course, "1111222233334444");
    RoundState state;
    TEST_ASSERT_TRUE(roundFromJson(json, state));
    TEST_ASSERT_EQUAL_STRING("1111222233334444", state.id);
    TEST_ASSERT_EQUAL_STRING("00aa11bb22cc33dd", state.course_id);
    TEST_ASSERT_EQUAL_STRING("00aa11bb22cc33dd", courseOf(state).id);
    // Und stabil: nochmals schreiben ergibt denselben Text.
    const Course again = courseOf(state);
    TEST_ASSERT_TRUE(json == roundToJson(state.round, state.hole, state.started, state.course_name, &again, state.id));
}

static void test_round_without_ids_stays_as_before() {
    const std::string json = roundToJson(Round(), 0, DateTime(), "X");
    TEST_ASSERT_TRUE(json.find("\"id\"") == std::string::npos);
    TEST_ASSERT_TRUE(json.find("course_id") == std::string::npos);
    RoundState state;
    TEST_ASSERT_TRUE(roundFromJson(json, state));
    TEST_ASSERT_EQUAL_STRING("", state.id);
    TEST_ASSERT_EQUAL_STRING("", state.course_id);
}

static void test_files_with_invalid_ids_are_rejected() {
    RoundState state;
    Course course;
    TEST_ASSERT_FALSE(roundFromJson(R"({"v":1,"id":"kurz","hole":0,"holes":[{"done":false,"shots":[]}]})", state));
    TEST_ASSERT_FALSE(roundFromJson(R"({"v":1,"id":7,"hole":0,"holes":[{"done":false,"shots":[]}]})", state));
    TEST_ASSERT_FALSE(roundFromJson(R"({"v":1,"course_id":"XYZ","hole":0,"holes":[{"done":false,"shots":[]}]})", state));
    TEST_ASSERT_FALSE(courseFromJson(R"({"v":1,"id":"0123456789ABCDEF","holes":[{"par":4}]})", course));
    TEST_ASSERT_FALSE(courseFromJson(R"({"v":1,"id":"0123456789abcdef","rev":3000000000,"holes":[{"par":4}]})", course));
    TEST_ASSERT_TRUE(courseFromJson(R"({"v":1,"id":"0123456789abcdef","rev":2000000000,"holes":[{"par":4}]})", course));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_ids);
    RUN_TEST(test_invalid_ids);
    RUN_TEST(test_make_id_uses_both_random_words);
    RUN_TEST(test_make_id_without_random_source_is_still_valid_and_distinct);
    RUN_TEST(test_id_from_content_is_stable_and_content_dependent);
    RUN_TEST(test_id_from_content_rarely_collides);
    RUN_TEST(test_course_id_and_revision_round_trip);
    RUN_TEST(test_course_without_id_writes_neither_id_nor_revision);
    RUN_TEST(test_round_ids_round_trip);
    RUN_TEST(test_round_without_ids_stays_as_before);
    RUN_TEST(test_files_with_invalid_ids_are_rejected);
    return UNITY_END();
}
