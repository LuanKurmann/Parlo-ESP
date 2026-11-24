#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "../memory_fs.h"
#include "../test_support.h"
#include "core/api.h"
#include "core/demo_course.h"

using namespace golf;
using testsupport::kTee;
using testsupport::MemoryFs;
using testsupport::offset;

void setUp() {}
void tearDown() {}

struct Rig {
    MemoryFs fs;
    std::unique_ptr<Session> session;

    explicit Rig(bool with_course = true) {
        if (with_course) Store(fs).saveCourse(demoCourse());
        session.reset(new Session(fs));
    }
    App& app() { return session->app(); }
    void restart() { session.reset(new Session(fs)); }

    ApiResponse call(const char* method, const char* path, const std::string& body = "") {
        ApiRequest request;
        request.method = method;
        request.path = path;
        request.body = body;
        return handleApi(*session, request);
    }

    // Schlag an der Stelle `along` Meter ab Abschlag des aktuellen Lochs.
    void hit(double along) {
        app().setPosition(positionOnHole(app().course().holes[app().currentHole()], along));
        app().button1Short();
        app().button1Short();
        session->sync();
    }
    void finishHole() {
        app().button2Short();
        app().button2Short();
        session->sync();
    }
    void endRoundAndStartNew() {
        app().button2Long();
        app().selectMenuItem(MenuItem::EndRound);
        app().button1Long();
        session->sync();
    }
};

static JsonDocument parse(const ApiResponse& response) {
    JsonDocument doc;
    TEST_ASSERT_TRUE_MESSAGE(deserializeJson(doc, response.body) == DeserializationError::Ok, response.body.c_str());
    return doc;
}

static void expectError(const ApiResponse& response, int status) {
    TEST_ASSERT_EQUAL_INT(status, response.status);
    TEST_ASSERT_EQUAL_STRING("application/json", response.content_type.c_str());
    JsonDocument doc = parse(response);
    TEST_ASSERT_TRUE(doc["error"].is<const char*>());
    TEST_ASSERT_TRUE(std::strlen(doc["error"].as<const char*>()) > 3);
}

// Kleiner gueltiger Platz mit drei Loechern.
static const char* kSmallCourse =
    R"({"v":1,"name":"Kurzplatz","holes":[{"par":3,"green":[473800000,85400000],"r":120,"tee":[473790000,85400000]},{"par":4},{"par":5}]})";

// ---------- Status ----------

static void test_status() {
    Rig rig;
    const ApiResponse response = rig.call("GET", "/api/status");
    TEST_ASSERT_EQUAL_INT(200, response.status);
    JsonDocument doc = parse(response);
    TEST_ASSERT_EQUAL_STRING("0.1.0", doc["version"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", doc["course"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(18, doc["holes"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, doc["current_hole"].as<int>());
    TEST_ASSERT_EQUAL_INT(0, doc["rounds"].as<int>());
    TEST_ASSERT_TRUE(doc["storage_ok"].as<bool>());
}

static void test_status_reports_storage_trouble() {
    Rig rig;
    rig.fs.read_only = true;
    rig.hit(0.0);
    JsonDocument doc = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_FALSE(doc["storage_ok"].as<bool>());
}

// ---------- Unbekanntes und Falsches ----------

static void test_unknown_paths_and_methods() {
    Rig rig;
    expectError(rig.call("GET", "/api/nix"), 404);
    expectError(rig.call("GET", "/api"), 404);
    expectError(rig.call("GET", "/"), 404);
    expectError(rig.call("GET", ""), 404);
    expectError(rig.call("GET", "/api/status/"), 404);
    expectError(rig.call("GET", "/api/course/../round"), 404);
    expectError(rig.call("POST", "/api/status"), 405);
    expectError(rig.call("DELETE", "/api/course"), 405);
    expectError(rig.call("PUT", "/api/round", "{}"), 405);
    expectError(rig.call("PUT", "/api/rounds", "{}"), 405);
    expectError(rig.call("POST", "/api/settings", "{}"), 405);
    expectError(rig.call("PUT", "/api/rounds/1", "{}"), 405); // Runden lassen sich nicht ueberschreiben
}

static void test_oversized_body_is_refused_before_anything_else() {
    Rig rig;
    expectError(rig.call("PUT", "/api/course", std::string(kMaxApiBodyBytes + 1, ' ')), 413);
    expectError(rig.call("GET", "/api/status", std::string(kMaxApiBodyBytes + 1, 'x')), 413);
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", rig.app().course().name);
}

// ---------- Platz ----------

static void test_get_course_returns_the_stored_format() {
    Rig rig;
    const ApiResponse response = rig.call("GET", "/api/course");
    TEST_ASSERT_EQUAL_INT(200, response.status);
    Course course;
    TEST_ASSERT_TRUE(courseFromJson(response.body, course));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", course.name);
    TEST_ASSERT_EQUAL_INT(342, holeLengthMeters(course.holes[0]));
}

static void test_get_course_without_any_course() {
    Rig rig(false);
    Course course;
    TEST_ASSERT_TRUE(courseFromJson(rig.call("GET", "/api/course").body, course));
    TEST_ASSERT_EQUAL_INT(18, course.hole_count);
    TEST_ASSERT_FALSE(course.holes[0].has_green);
}

static void test_put_course_replaces_and_persists() {
    Rig rig;
    const ApiResponse response = rig.call("PUT", "/api/course", kSmallCourse);
    TEST_ASSERT_EQUAL_INT(200, response.status);
    JsonDocument doc = parse(response);
    TEST_ASSERT_TRUE(doc["ok"].as<bool>());
    TEST_ASSERT_FALSE(doc["archived_round"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Kurzplatz", rig.app().course().name);
    TEST_ASSERT_EQUAL_INT(3, rig.app().round().holeCount());
    TEST_ASSERT_TRUE(rig.app().course().holes[0].has_green);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, rig.app().course().holes[0].green_radius_m);

    rig.restart();
    TEST_ASSERT_EQUAL_STRING("Kurzplatz", rig.app().course().name);
    TEST_ASSERT_EQUAL_INT(3, rig.app().round().holeCount());
    JsonDocument status = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_INT(3, status["holes"].as<int>());
}

static void test_put_course_round_trips_through_get() {
    Rig rig;
    rig.call("PUT", "/api/course", kSmallCourse);
    TEST_ASSERT_EQUAL_STRING(kSmallCourse, rig.call("GET", "/api/course").body.c_str());
}

static void test_put_invalid_course_changes_nothing() {
    Rig rig;
    rig.hit(0.0);
    const char* bad[] = {"", "kein json", "{}", R"({"v":1,"holes":[]})", R"({"v":1,"holes":[{"par":9}]})",
                         R"({"v":1,"holes":[{"par":4,"green":[999999999,0]}]})", R"({"v":2,"holes":[{"par":4}]})"};
    for (const char* body : bad) {
        expectError(rig.call("PUT", "/api/course", body), 400);
        TEST_ASSERT_EQUAL_STRING("Beispielplatz", rig.app().course().name);
        TEST_ASSERT_EQUAL_INT(1, rig.app().round().shotCount(0));
    }
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(rig.session->store().archivedRounds().size()));
}

static void test_put_course_archives_a_round_in_progress() {
    Rig rig;
    rig.hit(0.0);
    rig.hit(200.0);
    rig.finishHole();
    rig.hit(0.0);
    JsonDocument doc = parse(rig.call("PUT", "/api/course", kSmallCourse));
    TEST_ASSERT_TRUE(doc["archived_round"].as<bool>());

    // Neue, leere Runde auf dem neuen Platz ...
    TEST_ASSERT_EQUAL_INT(0, rig.app().currentHole());
    TEST_ASSERT_EQUAL_INT(0, rig.app().round().shotCount(0));
    // ... und die alte liegt mit ihren Platzdaten im Archiv.
    std::unique_ptr<RoundState> archived(new RoundState());
    TEST_ASSERT_TRUE(rig.session->store().loadArchivedRound(1, *archived));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", archived->course_name);
    TEST_ASSERT_EQUAL_INT(18, archived->round.holeCount());
    TEST_ASSERT_EQUAL_INT(2, archived->round.shotCount(0));
    TEST_ASSERT_EQUAL_INT(4, archived->par[0]);
    TEST_ASSERT_TRUE(archived->has_green[0]);
}

static void test_put_course_fails_cleanly_when_round_cannot_be_archived() {
    Rig rig;
    rig.hit(0.0);
    rig.fs.read_only = true;
    expectError(rig.call("PUT", "/api/course", kSmallCourse), 500);
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", rig.app().course().name); // nichts verloren
    TEST_ASSERT_EQUAL_INT(1, rig.app().round().shotCount(0));
}

static void test_put_course_closes_open_overlays() {
    Rig rig;
    rig.app().button1Short(); // Schlaegerwahl offen
    rig.call("PUT", "/api/course", kSmallCourse);
    TEST_ASSERT_TRUE(rig.app().overlay() == Overlay::None);
    TEST_ASSERT_EQUAL_INT(0, rig.app().round().shotCount(0));
}

// ---------- Laufende Runde ----------

static void test_get_current_round() {
    Rig rig;
    rig.hit(0.0);
    rig.hit(205.0);
    const ApiResponse response = rig.call("GET", "/api/round");
    TEST_ASSERT_EQUAL_INT(200, response.status);
    std::unique_ptr<RoundState> state(new RoundState());
    TEST_ASSERT_TRUE(roundFromJson(response.body, *state));
    TEST_ASSERT_EQUAL_INT(2, state->round.shotCount(0));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", state->course_name);
    TEST_ASSERT_EQUAL_INT(4, state->par[0]);
    TEST_ASSERT_EQUAL_INT(5, state->par[1]);
    TEST_ASSERT_TRUE(state->has_green[17]);
}

// ---------- Archiv ----------

static void playRound(Rig& rig, int strokes_on_hole_one, int putts) {
    for (int i = 0; i < strokes_on_hole_one - putts; i++) rig.hit(i * 100.0);
    for (int i = 0; i < putts; i++) rig.hit(340.0);
    rig.finishHole();
    rig.endRoundAndStartNew();
}

static void test_round_list_is_empty_at_first() {
    Rig rig;
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(doc["rounds"].size()));
}

static void test_round_list_newest_first_with_results() {
    Rig rig;
    DateTime when;
    when.valid = true;
    when.year = 2026;
    when.month = 11;
    when.day = 24;
    when.hour = 8;
    when.minute = 5;
    rig.session->setDateTime(when);
    playRound(rig, 5, 2); // Bogey
    playRound(rig, 3, 1); // Birdie
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    JsonArrayConst list = doc["rounds"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(list.size()));
    TEST_ASSERT_EQUAL_INT(2, list[0]["n"].as<int>());
    TEST_ASSERT_EQUAL_INT(3, list[0]["strokes"].as<int>());
    TEST_ASSERT_EQUAL_INT(-1, list[0]["to_par"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, list[0]["putts"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, list[1]["n"].as<int>());
    TEST_ASSERT_EQUAL_INT(5, list[1]["strokes"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, list[1]["to_par"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, list[1]["holes"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", list[1]["course"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("2026-11-24T08:05", list[1]["date"].as<const char*>());
}

static void test_round_list_without_date_and_without_par() {
    Rig rig(false); // Platz ohne Par
    rig.app().button1Short();
    rig.app().button1Short();
    rig.app().button2Short();
    rig.app().button2Short();
    rig.endRoundAndStartNew();
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    JsonObjectConst entry = doc["rounds"][0];
    TEST_ASSERT_TRUE(entry["date"].isNull());
    TEST_ASSERT_TRUE(entry["to_par"].isNull());
    TEST_ASSERT_EQUAL_INT(1, entry["strokes"].as<int>());
}

static void test_round_list_skips_damaged_files() {
    Rig rig;
    playRound(rig, 4, 2);
    playRound(rig, 5, 2);
    rig.fs.files["rounds/0001.json"][40] ^= 0x04;
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(doc["rounds"].size()));
    TEST_ASSERT_EQUAL_INT(2, doc["rounds"][0]["n"].as<int>());
    expectError(rig.call("GET", "/api/rounds/1"), 404);
}

static void test_get_single_round_keeps_its_course_data_after_course_change() {
    Rig rig;
    playRound(rig, 5, 2);
    rig.call("PUT", "/api/course", kSmallCourse); // Platz gewechselt
    const ApiResponse response = rig.call("GET", "/api/rounds/1");
    TEST_ASSERT_EQUAL_INT(200, response.status);
    std::unique_ptr<RoundState> state(new RoundState());
    TEST_ASSERT_TRUE(roundFromJson(response.body, *state));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", state->course_name);
    TEST_ASSERT_EQUAL_INT(5, state->round.shotCount(0));
    TEST_ASSERT_EQUAL_INT(4, state->par[0]);
    // Die Liste rechnet weiter mit dem Par von damals.
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(1, doc["rounds"][0]["to_par"].as<int>());
}

static void test_get_round_with_bad_numbers() {
    Rig rig;
    playRound(rig, 4, 2);
    const char* bad[] = {"/api/rounds/0", "/api/rounds/2", "/api/rounds/", "/api/rounds/abc", "/api/rounds/1x",
                         "/api/rounds/-1", "/api/rounds/00001", "/api/rounds/1/2", "/api/rounds/99999"};
    for (const char* path : bad) {
        const ApiResponse response = rig.call("GET", path);
        TEST_ASSERT_EQUAL_INT_MESSAGE(404, response.status, path);
    }
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/rounds/1").status);
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/rounds/0001").status);
}

static void test_delete_round() {
    Rig rig;
    playRound(rig, 4, 2);
    playRound(rig, 5, 2);
    TEST_ASSERT_EQUAL_INT(200, rig.call("DELETE", "/api/rounds/1").status);
    expectError(rig.call("GET", "/api/rounds/1"), 404);
    expectError(rig.call("DELETE", "/api/rounds/1"), 404); // schon weg
    JsonDocument doc = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(doc["rounds"].size()));
    TEST_ASSERT_EQUAL_INT(2, doc["rounds"][0]["n"].as<int>());
    rig.restart();
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(rig.session->store().archivedRounds().size()));
}

static void test_delete_does_not_touch_the_current_round() {
    Rig rig;
    playRound(rig, 4, 2);
    rig.hit(0.0);
    rig.call("DELETE", "/api/rounds/1");
    TEST_ASSERT_EQUAL_INT(1, rig.app().round().shotCount(0));
    rig.restart();
    TEST_ASSERT_EQUAL_INT(1, rig.app().round().shotCount(0));
}

static void test_delete_reports_storage_failure_as_not_deleted() {
    Rig rig;
    playRound(rig, 4, 2);
    rig.fs.read_only = true;
    TEST_ASSERT_TRUE(rig.call("DELETE", "/api/rounds/1").status != 200);
    rig.fs.read_only = false;
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", "/api/rounds/1").status);
}

// ---------- Schlagweiten ----------

static void test_get_settings_default_bag() {
    Rig rig;
    JsonDocument doc = parse(rig.call("GET", "/api/settings"));
    TEST_ASSERT_EQUAL_INT(1, doc["v"].as<int>());
    TEST_ASSERT_EQUAL_INT(210, doc["carry"]["Dr"].as<int>());
    TEST_ASSERT_EQUAL_INT(140, doc["carry"]["E7"].as<int>());
    TEST_ASSERT_EQUAL_INT(70, doc["carry"]["SW"].as<int>());
    TEST_ASSERT_TRUE(doc["carry"]["Pt"].isNull());
    TEST_ASSERT_EQUAL_INT(10, static_cast<int>(doc["carry"].size()));
}

static const char* kShortHitter =
    R"({"v":1,"carry":{"Dr":180,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})";

static void test_put_settings_changes_suggestions_and_persists() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", kShortHitter).status);
    TEST_ASSERT_EQUAL_STRING(kShortHitter, rig.call("GET", "/api/settings").body.c_str());

    rig.hit(0.0);
    rig.app().setPosition(positionOnHole(rig.app().course().holes[0], 202.0)); // 140 m bis zur Mitte
    rig.app().button1Short();
    TEST_ASSERT_TRUE(rig.app().suggestedClub() == Club::Iron5); // statt Eisen 7

    rig.restart();
    TEST_ASSERT_EQUAL_INT(180, rig.app().bag().carry_m[0]);
    TEST_ASSERT_EQUAL_INT(0, rig.app().bag().carry_m[static_cast<int>(Club::Putter)]);
}

static void test_put_invalid_settings_changes_nothing() {
    Rig rig;
    const char* bad[] = {
        "", "{}", R"({"v":1})", R"({"v":1,"carry":[]})",
        R"({"v":1,"carry":{"Dr":180}})", // Schlaeger fehlen
        R"({"v":1,"carry":{"Dr":0,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})",
        R"({"v":1,"carry":{"Dr":401,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})",
        R"({"v":1,"carry":{"Dr":"weit","H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})",
        R"({"v":1,"carry":{"Dr":180.5,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})",
    };
    for (const char* body : bad) {
        expectError(rig.call("PUT", "/api/settings", body), 400);
        TEST_ASSERT_EQUAL_INT(210, rig.app().bag().carry_m[0]);
    }
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(rig.fs.files.count("settings.json")));
}

static void test_put_settings_reports_storage_failure() {
    Rig rig;
    rig.fs.read_only = true;
    expectError(rig.call("PUT", "/api/settings", kShortHitter), 500);
    TEST_ASSERT_EQUAL_INT(210, rig.app().bag().carry_m[0]); // nicht halb uebernommen
}

static void test_damaged_settings_file_falls_back_to_defaults() {
    Rig rig;
    rig.call("PUT", "/api/settings", kShortHitter);
    rig.fs.files["settings.json"][25] ^= 0x01;
    rig.restart();
    TEST_ASSERT_EQUAL_INT(210, rig.app().bag().carry_m[0]);
}

// ---------- Zufaellige Anfragen ----------

static void test_random_requests_never_break_the_session() {
    Rig rig;
    const char* methods[] = {"GET", "PUT", "DELETE", "POST", "", "get"};
    const char* paths[] = {"/api/status", "/api/course", "/api/round", "/api/rounds", "/api/rounds/1", "/api/rounds/2",
                           "/api/settings", "/api/rounds/x", "/api/", "/"};
    const char* bodies[] = {"", "{}", kSmallCourse, kShortHitter, "[1,2,3]", "\"text\"", "{\"v\":1,\"holes\":[{\"par\":4}]}"};
    uint32_t state = 31;
    for (int i = 0; i < 3000; i++) {
        state = state * 1664525u + 1013904223u;
        const uint32_t r = state >> 8;
        if (r % 5 == 0) rig.hit(static_cast<double>(r % 300));
        if (r % 17 == 0) rig.finishHole();
        if (r % 41 == 0) rig.endRoundAndStartNew();
        const ApiResponse response = rig.call(methods[r % 6], paths[(r >> 3) % 10], bodies[(r >> 7) % 7]);
        TEST_ASSERT_TRUE(response.status == 200 || response.status == 400 || response.status == 404 || response.status == 405);
        JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc, response.body) == DeserializationError::Ok); // immer gueltiges JSON
        const int holes = rig.app().round().holeCount();
        TEST_ASSERT_TRUE(holes >= 1 && holes <= 18);
        TEST_ASSERT_TRUE(rig.app().currentHole() < holes);
        TEST_ASSERT_TRUE(rig.session->lastSyncOk());
    }
    rig.restart(); // und alles laesst sich danach wieder laden
    TEST_ASSERT_TRUE(rig.app().round().holeCount() >= 1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_status);
    RUN_TEST(test_status_reports_storage_trouble);
    RUN_TEST(test_unknown_paths_and_methods);
    RUN_TEST(test_oversized_body_is_refused_before_anything_else);
    RUN_TEST(test_get_course_returns_the_stored_format);
    RUN_TEST(test_get_course_without_any_course);
    RUN_TEST(test_put_course_replaces_and_persists);
    RUN_TEST(test_put_course_round_trips_through_get);
    RUN_TEST(test_put_invalid_course_changes_nothing);
    RUN_TEST(test_put_course_archives_a_round_in_progress);
    RUN_TEST(test_put_course_fails_cleanly_when_round_cannot_be_archived);
    RUN_TEST(test_put_course_closes_open_overlays);
    RUN_TEST(test_get_current_round);
    RUN_TEST(test_round_list_is_empty_at_first);
    RUN_TEST(test_round_list_newest_first_with_results);
    RUN_TEST(test_round_list_without_date_and_without_par);
    RUN_TEST(test_round_list_skips_damaged_files);
    RUN_TEST(test_get_single_round_keeps_its_course_data_after_course_change);
    RUN_TEST(test_get_round_with_bad_numbers);
    RUN_TEST(test_delete_round);
    RUN_TEST(test_delete_does_not_touch_the_current_round);
    RUN_TEST(test_delete_reports_storage_failure_as_not_deleted);
    RUN_TEST(test_get_settings_default_bag);
    RUN_TEST(test_put_settings_changes_suggestions_and_persists);
    RUN_TEST(test_put_invalid_settings_changes_nothing);
    RUN_TEST(test_put_settings_reports_storage_failure);
    RUN_TEST(test_damaged_settings_file_falls_back_to_defaults);
    RUN_TEST(test_random_requests_never_break_the_session);
    return UNITY_END();
}
