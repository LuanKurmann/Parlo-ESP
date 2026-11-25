// Prueft das Dateiformat gegen die gemeinsamen Testdaten in testdata/format.
// Dieselben Dateien liest die App (app/test/model/format_files_test.dart).
#include <cstdio>
#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "core/serialize.h"

using namespace golf;

void setUp() {}
void tearDown() {}

static std::string readFile(const char* name) {
    const std::string path = std::string("testdata/format/") + name;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
    std::string out;
    char buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
    std::fclose(f);
    return out;
}

// Liest `json` als `kind` und schreibt es wieder. false, wenn abgelehnt.
static bool rewrite(const std::string& kind, const std::string& json, std::string& out) {
    if (kind == "course") {
        Course course;
        if (!courseFromJson(json, course)) return false;
        out = courseToJson(course);
        return true;
    }
    if (kind == "round") {
        RoundState state;
        if (!roundFromJson(json, state)) return false;
        const Course course = courseOf(state);
        out = roundToJson(state.round, state.hole, state.started, state.course_name, &course, state.id);
        return true;
    }
    if (kind == "settings") {
        Bag bag;
        DeviceSettings device;
        if (!settingsFromJson(json, bag, device)) return false;
        out = settingsToJson(bag, device);
        return true;
    }
    TEST_FAIL_MESSAGE(("unbekannte Art: " + kind).c_str());
    return false;
}

static void expectUnchanged(const char* kind, const char* file) {
    const std::string json = readFile(file);
    std::string out;
    TEST_ASSERT_TRUE_MESSAGE(rewrite(kind, json, out), file);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(json.c_str(), out.c_str(), file);
}

void test_canonical_files_are_rewritten_unchanged() {
    expectUnchanged("course", "course.json");
    expectUnchanged("round", "round.json");
    expectUnchanged("settings", "settings.json");
    expectUnchanged("settings", "settings_default.json");
}

void test_default_settings_match_the_file() {
    TEST_ASSERT_EQUAL_STRING(readFile("settings_default.json").c_str(),
                             settingsToJson(Bag(), DeviceSettings()).c_str());
}

void test_rejects() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, readFile("rejects.json")) == DeserializationError::Ok);
    int count = 0;
    for (JsonObjectConst item : doc.as<JsonArrayConst>()) {
        const std::string kind = item["kind"].as<const char*>();
        const std::string why = kind + ": " + item["why"].as<const char*>();
        std::string out;
        TEST_ASSERT_FALSE_MESSAGE(rewrite(kind, item["json"].as<std::string>(), out), why.c_str());
        count++;
    }
    TEST_ASSERT_GREATER_THAN(60, count);
}

void test_accepts() {
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, readFile("accepts.json")) == DeserializationError::Ok);
    int count = 0;
    for (JsonObjectConst item : doc.as<JsonArrayConst>()) {
        const std::string kind = item["kind"].as<const char*>();
        const std::string why = kind + ": " + item["why"].as<const char*>();
        std::string out;
        TEST_ASSERT_TRUE_MESSAGE(rewrite(kind, item["json"].as<std::string>(), out), why.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(item["canonical"].as<const char*>(), out.c_str(), why.c_str());
        count++;
    }
    TEST_ASSERT_GREATER_THAN(8, count);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_canonical_files_are_rewritten_unchanged);
    RUN_TEST(test_default_settings_match_the_file);
    RUN_TEST(test_rejects);
    RUN_TEST(test_accepts);
    return UNITY_END();
}
