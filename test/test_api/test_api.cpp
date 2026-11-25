#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <ArduinoJson.h>
#include <unity.h>

#include "../memory_fs.h"
#include "../test_support.h"
#include "core/api.h"
#include "core/demo_course.h"
#include "core/safe_file.h"

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
    TEST_ASSERT_EQUAL_INT(3, doc["api"].as<int>());
    TEST_ASSERT_TRUE(isValidId(doc["device_id"].as<const char*>()));
    TEST_ASSERT_TRUE(isValidId(doc["course_id"].as<const char*>()));
    TEST_ASSERT_EQUAL_STRING("Beispielplatz", doc["course"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(18, doc["holes"].as<int>());
    TEST_ASSERT_EQUAL_INT(1, doc["current_hole"].as<int>());
    TEST_ASSERT_EQUAL_INT(0, doc["rounds"].as<int>());
    TEST_ASSERT_TRUE(doc["storage_ok"].as<bool>());
}

static void test_status_reports_battery_only_when_known() {
    Rig rig;
    JsonDocument doc = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_TRUE(doc["battery"].isNull());
    BatteryInfo battery;
    battery.known = true;
    battery.percent = 64;
    battery.charging = true;
    rig.app().setBattery(battery);
    doc = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_INT(64, doc["battery"].as<int>());
    TEST_ASSERT_TRUE(doc["charging"].as<bool>());
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

// Derselbe Platz, wie ihn die App schickt: mit Kennung und Aenderungszaehler.
static const char* kSmallCourseWithId =
    R"({"v":1,"id":"00aa11bb22cc33dd","rev":7,"name":"Kurzplatz","holes":[{"par":3,"green":[473800000,85400000],"r":120,"tee":[473790000,85400000]},{"par":4},{"par":5}]})";

static void test_put_course_with_id_round_trips_through_get() {
    Rig rig;
    rig.call("PUT", "/api/course", kSmallCourseWithId);
    TEST_ASSERT_EQUAL_STRING(kSmallCourseWithId, rig.call("GET", "/api/course").body.c_str());
    JsonDocument status = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_STRING("00aa11bb22cc33dd", status["course_id"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(7, status["course_rev"].as<int>());
}

static void test_put_course_without_id_gets_one() {
    Rig rig;
    rig.call("PUT", "/api/course", kSmallCourse);
    JsonDocument doc = parse(rig.call("GET", "/api/course"));
    TEST_ASSERT_TRUE(isValidId(doc["id"].as<const char*>()));
    TEST_ASSERT_EQUAL_INT(0, doc["rev"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Kurzplatz", doc["name"].as<const char*>());
    const std::string first_id = doc["id"].as<const char*>();
    rig.restart();
    doc = parse(rig.call("GET", "/api/course"));
    TEST_ASSERT_EQUAL_STRING(first_id.c_str(), doc["id"].as<const char*>()); // bleibt gleich
}

static void test_put_course_with_invalid_id_is_rejected() {
    Rig rig;
    const char* bad[] = {
        R"({"v":1,"id":"kurz","holes":[{"par":4}]})", R"({"v":1,"id":"00AA11BB22CC33DD","holes":[{"par":4}]})",
        R"({"v":1,"id":"00aa11bb22cc33ddee","holes":[{"par":4}]})", R"({"v":1,"id":12345,"holes":[{"par":4}]})",
        R"({"v":1,"id":"00aa11bb22cc33dd","rev":-1,"holes":[{"par":4}]})",
        R"({"v":1,"id":"00aa11bb22cc33dd","rev":"7","holes":[{"par":4}]})",
    };
    for (const char* body : bad) expectError(rig.call("PUT", "/api/course", body), 400);
}

static void test_marking_a_green_raises_the_course_revision() {
    Rig rig;
    rig.call("PUT", "/api/course", kSmallCourseWithId);
    rig.app().setPosition(kTee);
    rig.app().button2Long();
    rig.app().selectMenuItem(MenuItem::MarkGreen);
    rig.session->sync();
    JsonDocument status = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_INT(8, status["course_rev"].as<int>());
    TEST_ASSERT_EQUAL_STRING("00aa11bb22cc33dd", status["course_id"].as<const char*>()); // Kennung bleibt
    // Ohne GPS wird nichts markiert, also auch nichts gezaehlt.
    rig.app().clearPosition();
    rig.app().button2Long();
    rig.app().selectMenuItem(MenuItem::MarkGreen);
    status = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_INT(8, status["course_rev"].as<int>());
    rig.restart();
    status = parse(rig.call("GET", "/api/status"));
    TEST_ASSERT_EQUAL_INT(8, status["course_rev"].as<int>());
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

// ---------- Dauerhafte Kennungen ----------

static void test_device_id_is_created_once_and_survives_restarts() {
    Rig rig;
    const std::string id = parse(rig.call("GET", "/api/status"))["device_id"].as<const char*>();
    rig.session->sync();
    rig.restart();
    TEST_ASSERT_EQUAL_STRING(id.c_str(), parse(rig.call("GET", "/api/status"))["device_id"].as<const char*>());
    rig.call("PUT", "/api/course", kSmallCourse); // ein Platzwechsel aendert sie nicht
    rig.restart();
    TEST_ASSERT_EQUAL_STRING(id.c_str(), parse(rig.call("GET", "/api/status"))["device_id"].as<const char*>());
}

static void test_damaged_device_file_gives_a_new_valid_id() {
    Rig rig;
    rig.session->sync();
    rig.fs.files["device.json"] = wrapPayload("{\"v\":1,\"id\":\"nicht gueltig!!!\"}");
    rig.restart();
    TEST_ASSERT_TRUE(isValidId(parse(rig.call("GET", "/api/status"))["device_id"].as<const char*>()));
    TEST_ASSERT_TRUE(rig.session->sync());
}

static void test_each_round_has_its_own_id_which_it_keeps_in_the_archive() {
    Rig rig;
    rig.hit(0.0);
    const std::string first = parse(rig.call("GET", "/api/round"))["id"].as<const char*>();
    TEST_ASSERT_TRUE(isValidId(first.c_str()));
    rig.restart(); // die laufende Runde behaelt ihre Kennung ueber einen Neustart
    TEST_ASSERT_EQUAL_STRING(first.c_str(), parse(rig.call("GET", "/api/round"))["id"].as<const char*>());

    rig.endRoundAndStartNew();
    const std::string second = parse(rig.call("GET", "/api/round"))["id"].as<const char*>();
    TEST_ASSERT_TRUE(isValidId(second.c_str()));
    TEST_ASSERT_TRUE(first != second);

    JsonDocument list = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_STRING(first.c_str(), list["rounds"][0]["id"].as<const char*>());
    JsonDocument archived = parse(rig.call("GET", "/api/rounds/1"));
    TEST_ASSERT_EQUAL_STRING(first.c_str(), archived["id"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING(rig.app().course().id, archived["course_id"].as<const char*>());
}

static void test_round_archived_by_course_change_keeps_its_id() {
    Rig rig;
    rig.hit(0.0);
    const std::string id = parse(rig.call("GET", "/api/round"))["id"].as<const char*>();
    const std::string old_course = rig.app().course().id;
    rig.call("PUT", "/api/course", kSmallCourseWithId);
    JsonDocument archived = parse(rig.call("GET", "/api/rounds/1"));
    TEST_ASSERT_EQUAL_STRING(id.c_str(), archived["id"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING(old_course.c_str(), archived["course_id"].as<const char*>()); // der Platz von damals
    TEST_ASSERT_TRUE(id != parse(rig.call("GET", "/api/round"))["id"].as<const char*>());
}

static void test_many_rounds_get_distinct_ids() {
    Rig rig;
    std::vector<std::string> ids;
    for (int i = 0; i < 30; i++) {
        rig.hit(0.0);
        rig.endRoundAndStartNew();
    }
    JsonDocument list = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(30, static_cast<int>(list["rounds"].size()));
    for (JsonVariantConst entry : list["rounds"].as<JsonArrayConst>()) {
        const std::string id = entry["id"].as<const char*>();
        TEST_ASSERT_TRUE(isValidId(id.c_str()));
        for (const std::string& seen : ids) TEST_ASSERT_TRUE(seen != id);
        ids.push_back(id);
    }
}

static void test_get_and_delete_round_by_id() {
    Rig rig;
    playRound(rig, 4, 2);
    playRound(rig, 5, 2);
    JsonDocument list = parse(rig.call("GET", "/api/rounds"));
    const std::string newer = list["rounds"][0]["id"].as<const char*>();
    const std::string older = list["rounds"][1]["id"].as<const char*>();

    const ApiResponse got = rig.call("GET", ("/api/rounds/id/" + older).c_str());
    TEST_ASSERT_EQUAL_INT(200, got.status);
    std::unique_ptr<RoundState> state(new RoundState());
    TEST_ASSERT_TRUE(roundFromJson(got.body, *state));
    TEST_ASSERT_EQUAL_INT(4, state->round.shotCount(0));
    TEST_ASSERT_EQUAL_STRING(older.c_str(), state->id);

    TEST_ASSERT_EQUAL_INT(200, rig.call("DELETE", ("/api/rounds/id/" + older).c_str()).status);
    expectError(rig.call("GET", ("/api/rounds/id/" + older).c_str()), 404);
    expectError(rig.call("DELETE", ("/api/rounds/id/" + older).c_str()), 404);
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", ("/api/rounds/id/" + newer).c_str()).status); // die andere ist noch da
}

static void test_reused_number_cannot_be_confused_thanks_to_ids() {
    // Runde 1 wird geloescht, danach bekommt eine neue Runde wieder die Nummer 1.
    Rig rig;
    playRound(rig, 4, 2);
    const std::string old_id = parse(rig.call("GET", "/api/rounds"))["rounds"][0]["id"].as<const char*>();
    rig.call("DELETE", "/api/rounds/1");
    playRound(rig, 6, 3);
    JsonDocument list = parse(rig.call("GET", "/api/rounds"));
    TEST_ASSERT_EQUAL_INT(1, list["rounds"][0]["n"].as<int>());
    TEST_ASSERT_TRUE(old_id != list["rounds"][0]["id"].as<const char*>());
    expectError(rig.call("GET", ("/api/rounds/id/" + old_id).c_str()), 404); // die alte ist wirklich weg
}

static void test_invalid_ids_in_the_path() {
    Rig rig;
    playRound(rig, 4, 2);
    const char* bad[] = {"/api/rounds/id/", "/api/rounds/id/1", "/api/rounds/id/zzzzzzzzzzzzzzzz", "/api/rounds/id/00AA11BB22CC33DD",
                         "/api/rounds/id/0000000000000000", "/api/rounds/id/00aa11bb22cc33dd/x", "/api/rounds/id"};
    for (const char* path : bad) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(404, rig.call("GET", path).status, path);
        TEST_ASSERT_EQUAL_INT_MESSAGE(404, rig.call("DELETE", path).status, path);
    }
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(rig.session->store().archivedRounds().size()));
}

static void test_old_round_file_without_id_gets_a_stable_one() {
    // Datei aus einer frueheren Firmware: ohne Kennung.
    Rig rig;
    Round round;
    round.addShotWithoutPosition(0);
    round.setClub(0, 0, Club::Driver);
    round.finishHole(0);
    rig.session->store().archiveRound(round, DateTime(), "Alt");
    TEST_ASSERT_TRUE(rig.fs.files["rounds/0001.json"].find("\"id\"") == std::string::npos);

    const std::string id = parse(rig.call("GET", "/api/rounds"))["rounds"][0]["id"].as<const char*>();
    TEST_ASSERT_TRUE(isValidId(id.c_str()));
    rig.restart();
    TEST_ASSERT_EQUAL_STRING(id.c_str(), parse(rig.call("GET", "/api/rounds"))["rounds"][0]["id"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING(id.c_str(), parse(rig.call("GET", "/api/rounds/1"))["id"].as<const char*>());
    TEST_ASSERT_EQUAL_INT(200, rig.call("GET", ("/api/rounds/id/" + id).c_str()).status);
}

// ---------- Schlagweiten und Bag ----------

static void test_get_settings_default_bag() {
    Rig rig;
    JsonDocument doc = parse(rig.call("GET", "/api/settings"));
    TEST_ASSERT_EQUAL_INT(1, doc["v"].as<int>());
    TEST_ASSERT_EQUAL_INT(210, doc["carry"]["Dr"].as<int>());
    TEST_ASSERT_EQUAL_INT(140, doc["carry"]["E7"].as<int>());
    TEST_ASSERT_EQUAL_INT(85, doc["carry"]["GW"].as<int>()); // auch Schlaeger, die nicht im Bag sind
    TEST_ASSERT_TRUE(doc["carry"]["Pt"].isNull());
    TEST_ASSERT_EQUAL_INT(16, static_cast<int>(doc["carry"].size()));
    JsonArrayConst clubs = doc["clubs"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL_INT(11, static_cast<int>(clubs.size()));
    TEST_ASSERT_EQUAL_STRING("Dr", clubs[0].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Hy", clubs[2].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Pt", clubs[10].as<const char*>());
}

// So schickt es die Web-Oberflaeche der ersten Fassung: nur Schlagweiten.
static const char* kShortHitter =
    R"({"v":1,"carry":{"Dr":180,"H3":165,"Hy":150,"E5":135,"E6":125,"E7":115,"E8":105,"E9":95,"PW":85,"SW":60}})";

static void test_put_settings_changes_suggestions_and_persists() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", kShortHitter).status);
    JsonDocument doc = parse(rig.call("GET", "/api/settings"));
    TEST_ASSERT_EQUAL_INT(180, doc["carry"]["Dr"].as<int>());
    TEST_ASSERT_EQUAL_INT(60, doc["carry"]["SW"].as<int>());
    TEST_ASSERT_EQUAL_INT(85, doc["carry"]["GW"].as<int>());                 // nicht genannt: bleibt
    TEST_ASSERT_EQUAL_INT(11, static_cast<int>(doc["clubs"].size()));        // Auswahl bleibt

    rig.hit(0.0);
    rig.app().setPosition(positionOnHole(rig.app().course().holes[0], 202.0)); // 140 m bis zur Mitte
    rig.app().button1Short();
    TEST_ASSERT_TRUE(rig.app().suggestedClub() == Club::Iron5); // statt Eisen 7

    rig.restart();
    TEST_ASSERT_EQUAL_INT(180, rig.app().bag().carry_m[0]);
    TEST_ASSERT_EQUAL_INT(0, rig.app().bag().carry_m[static_cast<int>(Club::Putter)]);
}

static void test_put_settings_round_trips_through_get() {
    Rig rig;
    const std::string original = rig.call("GET", "/api/settings").body;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", original).status);
    TEST_ASSERT_EQUAL_STRING(original.c_str(), rig.call("GET", "/api/settings").body.c_str());
}

static void test_put_club_selection() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", R"({"v":1,"clubs":["H5","E4","E8","GW","Pt"]})").status);
    JsonDocument doc = parse(rig.call("GET", "/api/settings"));
    TEST_ASSERT_EQUAL_INT(5, static_cast<int>(doc["clubs"].size()));
    TEST_ASSERT_EQUAL_STRING("H5", doc["clubs"][0].as<const char*>());
    TEST_ASSERT_EQUAL_INT(210, doc["carry"]["Dr"].as<int>()); // Schlagweiten bleiben, auch fuer abgewaehlte

    // Der Vorschlag am Geraet kommt nur noch aus diesen Schlaegern.
    rig.app().setPosition(rig.app().course().holes[0].tee);
    rig.app().button1Short();
    TEST_ASSERT_TRUE(rig.app().suggestedClub() == Club::Wood5);
    rig.app().button2Short();
    TEST_ASSERT_TRUE(rig.app().suggestedClub() == Club::Iron4);

    rig.restart();
    TEST_ASSERT_FALSE(rig.app().bag().in_bag[static_cast<int>(Club::Driver)]);
    TEST_ASSERT_TRUE(rig.app().bag().in_bag[static_cast<int>(Club::GapWedge)]);
}

static void test_putter_stays_even_if_the_list_omits_it() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", R"({"v":1,"clubs":["Dr","E7"]})").status);
    JsonDocument doc = parse(rig.call("GET", "/api/settings"));
    TEST_ASSERT_EQUAL_INT(3, static_cast<int>(doc["clubs"].size()));
    TEST_ASSERT_EQUAL_STRING("Pt", doc["clubs"][2].as<const char*>());
}

static void test_selection_and_distances_in_one_request() {
    Rig rig;
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", R"({"v":1,"carry":{"LW":48},"clubs":["Dr","LW"]})").status);
    TEST_ASSERT_EQUAL_INT(48, rig.app().bag().carry_m[static_cast<int>(Club::LobWedge)]);
    TEST_ASSERT_TRUE(rig.app().bag().in_bag[static_cast<int>(Club::LobWedge)]);
    TEST_ASSERT_FALSE(rig.app().bag().in_bag[static_cast<int>(Club::Iron7)]);
}

static void test_full_bag_of_14_is_accepted_and_15_is_not() {
    Rig rig;
    const char* fourteen = R"({"v":1,"clubs":["Dr","H3","H5","Hy","E4","E5","E6","E7","E8","E9","PW","GW","SW","Pt"]})";
    const char* fifteen = R"({"v":1,"clubs":["Dr","H3","H5","Hy","E4","E5","E6","E7","E8","E9","PW","GW","SW","LW","Pt"]})";
    TEST_ASSERT_EQUAL_INT(200, rig.call("PUT", "/api/settings", fourteen).status);
    expectError(rig.call("PUT", "/api/settings", fifteen), 400);
    TEST_ASSERT_EQUAL_INT(14, bagSize(rig.app().bag())); // die 14 bleiben
    // 14 Schlaeger ohne genannten Putter waeren mit Putter 15.
    expectError(rig.call("PUT", "/api/settings", R"({"v":1,"clubs":["Dr","H3","H5","H7","Hy","E3","E4","E5","E6","E7","E8","E9","PW","GW"]})"), 400);
}

static void test_put_invalid_settings_changes_nothing() {
    Rig rig;
    const char* bad[] = {
        "", "{}", R"({"v":1})", R"({"v":1,"carry":[]})", R"({"v":2,"carry":{"Dr":180}})",
        R"({"v":1,"carry":{"Dr":0}})", R"({"v":1,"carry":{"Dr":401}})", R"({"v":1,"carry":{"Dr":"weit"}})",
        R"({"v":1,"carry":{"Dr":180.5}})",
        R"({"v":1,"carry":{"XX":150}})",        // unbekannter Schlaeger
        R"({"v":1,"carry":{"Pt":5}})",          // der Putter hat keine Schlagweite
        R"({"v":1,"clubs":"Dr"})", R"({"v":1,"clubs":[]})", // leer waere nur der Putter
        R"({"v":1,"clubs":["Pt"]})",
        R"({"v":1,"clubs":["Dr","Dr"]})",       // doppelt
        R"({"v":1,"clubs":["Dr","ZZ"]})", R"({"v":1,"clubs":["Dr",7]})", R"({"v":1,"clubs":["dr"]})",
        R"({"v":1,"carry":{"Dr":180},"clubs":["Dr","ZZ"]})", // ein Fehler verwirft die ganze Anfrage
    };
    for (const char* body : bad) {
        expectError(rig.call("PUT", "/api/settings", body), 400);
        TEST_ASSERT_EQUAL_INT_MESSAGE(210, rig.app().bag().carry_m[0], body);
        TEST_ASSERT_EQUAL_INT_MESSAGE(11, bagSize(rig.app().bag()), body);
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

static void test_settings_file_of_the_first_version_is_still_read() {
    // Datei mit nur zehn Schlagweiten und ohne Schlaegerliste.
    Rig rig;
    rig.fs.files["settings.json"] = wrapPayload(kShortHitter);
    rig.restart();
    TEST_ASSERT_EQUAL_INT(180, rig.app().bag().carry_m[0]);
    TEST_ASSERT_EQUAL_INT(11, bagSize(rig.app().bag()));
}

static void test_shots_with_clubs_no_longer_in_the_bag_stay_readable() {
    Rig rig;
    rig.hit(0.0); // Driver
    rig.endRoundAndStartNew();
    rig.call("PUT", "/api/settings", R"({"v":1,"clubs":["E7"]})"); // Driver ist nicht mehr im Bag
    rig.restart();
    std::unique_ptr<RoundState> state(new RoundState());
    TEST_ASSERT_TRUE(roundFromJson(rig.call("GET", "/api/rounds/1").body, *state));
    TEST_ASSERT_TRUE(state->round.shot(0, 0)->club == Club::Driver);
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

// ---------- WLAN ----------

// Attrappe fuer das WLAN des Geraets.
class FakeNetwork : public NetworkControl {
public:
    NetStatus current;
    std::string saved_ssid, saved_password;
    bool fail = false;
    int saves = 0, forgets = 0;

    FakeNetwork() {
        current.mode = NetMode::Hotspot;
        std::snprintf(current.ssid, sizeof(current.ssid), "Golf-Tracker");
        std::snprintf(current.address, sizeof(current.address), "192.168.4.1");
        std::snprintf(current.hotspot_password, sizeof(current.hotspot_password), "48213975");
    }
    NetStatus status() const override { return current; }
    bool saveNetwork(const char* ssid, const char* password) override {
        if (fail) return false;
        saves++;
        saved_ssid = ssid;
        saved_password = password;
        current.has_saved_network = true;
        return true;
    }
    bool forgetNetwork() override {
        if (fail) return false;
        forgets++;
        saved_ssid.clear();
        saved_password.clear();
        current.has_saved_network = false;
        return true;
    }
};

static ApiResponse callNet(Rig& rig, FakeNetwork& network, const char* method, const std::string& body = "") {
    ApiRequest request;
    request.method = method;
    request.path = "/api/wifi";
    request.body = body;
    return handleApi(*rig.session, request, &network);
}

static void test_wifi_endpoint_is_absent_without_network_control() {
    Rig rig;
    expectError(rig.call("GET", "/api/wifi"), 404);
    expectError(rig.call("PUT", "/api/wifi", R"({"ssid":"X","password":"12345678"})"), 404);
}

static void test_get_wifi_status_never_contains_the_password() {
    Rig rig;
    FakeNetwork network;
    callNet(rig, network, "PUT", R"({"ssid":"Heimnetz","password":"streng geheim 123"})");
    const ApiResponse response = callNet(rig, network, "GET");
    TEST_ASSERT_EQUAL_INT(200, response.status);
    JsonDocument doc = parse(response);
    TEST_ASSERT_EQUAL_STRING("hotspot", doc["mode"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Golf-Tracker", doc["ssid"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", doc["address"].as<const char*>());
    TEST_ASSERT_TRUE(doc["saved"].as<bool>());
    TEST_ASSERT_TRUE(response.body.find("geheim") == std::string::npos);
    TEST_ASSERT_TRUE(response.body.find("48213975") == std::string::npos); // auch nicht das des Hotspots
    TEST_ASSERT_TRUE(doc["password"].isNull());
}

static void test_wifi_modes_are_reported() {
    Rig rig;
    FakeNetwork network;
    const struct {
        NetMode mode;
        const char* name;
    } cases[] = {{NetMode::Off, "off"}, {NetMode::Connecting, "connecting"}, {NetMode::Hotspot, "hotspot"}, {NetMode::Connected, "connected"}};
    for (const auto& c : cases) {
        network.current.mode = c.mode;
        JsonDocument doc = parse(callNet(rig, network, "GET"));
        TEST_ASSERT_EQUAL_STRING(c.name, doc["mode"].as<const char*>());
    }
}

static void test_put_wifi_saves_name_and_password() {
    Rig rig;
    FakeNetwork network;
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "PUT", R"({"ssid":"Heimnetz Müller","password":"p4ss wort!"})").status);
    TEST_ASSERT_EQUAL_STRING("Heimnetz Müller", network.saved_ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("p4ss wort!", network.saved_password.c_str());
    // Offenes WLAN: leeres oder fehlendes Passwort.
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "PUT", R"({"ssid":"Offen","password":""})").status);
    TEST_ASSERT_EQUAL_STRING("", network.saved_password.c_str());
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "PUT", R"({"ssid":"Offen2"})").status);
    TEST_ASSERT_EQUAL_STRING("Offen2", network.saved_ssid.c_str());
    // Grenzen: 32 Zeichen Name, 8 und 63 Zeichen Passwort.
    const std::string longest = R"({"ssid":")" + std::string(32, 'n') + R"(","password":")" + std::string(63, 'p') + R"("})";
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "PUT", longest).status);
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "PUT", R"({"ssid":"X","password":"12345678"})").status);
}

static void test_put_invalid_wifi_is_rejected() {
    Rig rig;
    FakeNetwork network;
    const std::string too_long_name = R"({"ssid":")" + std::string(33, 'n') + R"(","password":"12345678"})";
    const std::string too_long_password = R"({"ssid":"X","password":")" + std::string(64, 'p') + R"("})";
    const std::string bad[] = {
        "", "kein json", "[]", "{}", R"({"password":"12345678"})", R"({"ssid":"","password":"12345678"})",
        R"({"ssid":5,"password":"12345678"})", R"({"ssid":"X","password":"1234567"})", // 7 Zeichen
        R"({"ssid":"X","password":12345678})", R"({"ssid":"Zeile\numbruch","password":"12345678"})",
        R"({"ssid":"X","password":"tab\tdrin12"})", too_long_name, too_long_password,
    };
    for (const std::string& body : bad) {
        const ApiResponse response = callNet(rig, network, "PUT", body);
        TEST_ASSERT_EQUAL_INT_MESSAGE(400, response.status, body.c_str());
    }
    TEST_ASSERT_EQUAL_INT(0, network.saves);
}

static void test_delete_wifi_forgets_the_network() {
    Rig rig;
    FakeNetwork network;
    callNet(rig, network, "PUT", R"({"ssid":"Heimnetz","password":"12345678"})");
    TEST_ASSERT_EQUAL_INT(200, callNet(rig, network, "DELETE").status);
    TEST_ASSERT_EQUAL_INT(1, network.forgets);
    JsonDocument doc = parse(callNet(rig, network, "GET"));
    TEST_ASSERT_FALSE(doc["saved"].as<bool>());
}

static void test_wifi_storage_failure_and_wrong_method() {
    Rig rig;
    FakeNetwork network;
    network.fail = true;
    expectError(callNet(rig, network, "PUT", R"({"ssid":"Heimnetz","password":"12345678"})"), 500);
    expectError(callNet(rig, network, "DELETE"), 500);
    expectError(callNet(rig, network, "POST", "{}"), 405);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_status);
    RUN_TEST(test_status_reports_battery_only_when_known);
    RUN_TEST(test_status_reports_storage_trouble);
    RUN_TEST(test_unknown_paths_and_methods);
    RUN_TEST(test_oversized_body_is_refused_before_anything_else);
    RUN_TEST(test_get_course_returns_the_stored_format);
    RUN_TEST(test_get_course_without_any_course);
    RUN_TEST(test_put_course_replaces_and_persists);
    RUN_TEST(test_put_course_with_id_round_trips_through_get);
    RUN_TEST(test_put_course_without_id_gets_one);
    RUN_TEST(test_put_course_with_invalid_id_is_rejected);
    RUN_TEST(test_marking_a_green_raises_the_course_revision);
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
    RUN_TEST(test_device_id_is_created_once_and_survives_restarts);
    RUN_TEST(test_damaged_device_file_gives_a_new_valid_id);
    RUN_TEST(test_each_round_has_its_own_id_which_it_keeps_in_the_archive);
    RUN_TEST(test_round_archived_by_course_change_keeps_its_id);
    RUN_TEST(test_many_rounds_get_distinct_ids);
    RUN_TEST(test_get_and_delete_round_by_id);
    RUN_TEST(test_reused_number_cannot_be_confused_thanks_to_ids);
    RUN_TEST(test_invalid_ids_in_the_path);
    RUN_TEST(test_old_round_file_without_id_gets_a_stable_one);
    RUN_TEST(test_get_settings_default_bag);
    RUN_TEST(test_put_settings_changes_suggestions_and_persists);
    RUN_TEST(test_put_settings_round_trips_through_get);
    RUN_TEST(test_put_club_selection);
    RUN_TEST(test_putter_stays_even_if_the_list_omits_it);
    RUN_TEST(test_selection_and_distances_in_one_request);
    RUN_TEST(test_full_bag_of_14_is_accepted_and_15_is_not);
    RUN_TEST(test_settings_file_of_the_first_version_is_still_read);
    RUN_TEST(test_shots_with_clubs_no_longer_in_the_bag_stay_readable);
    RUN_TEST(test_put_invalid_settings_changes_nothing);
    RUN_TEST(test_put_settings_reports_storage_failure);
    RUN_TEST(test_damaged_settings_file_falls_back_to_defaults);
    RUN_TEST(test_random_requests_never_break_the_session);
    RUN_TEST(test_wifi_endpoint_is_absent_without_network_control);
    RUN_TEST(test_get_wifi_status_never_contains_the_password);
    RUN_TEST(test_wifi_modes_are_reported);
    RUN_TEST(test_put_wifi_saves_name_and_password);
    RUN_TEST(test_put_invalid_wifi_is_rejected);
    RUN_TEST(test_delete_wifi_forgets_the_network);
    RUN_TEST(test_wifi_storage_failure_and_wrong_method);
    return UNITY_END();
}
