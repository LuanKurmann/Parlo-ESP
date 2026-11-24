#include <cstdio>
#include <cstring>
#include <memory>

#include <unity.h>

#include "../memory_fs.h"
#include "../test_support.h"
#include "core/safe_file.h"
#include "core/session.h"

using namespace golf;
using testsupport::kTee;
using testsupport::MemoryFs;
using testsupport::offset;

void setUp() {}
void tearDown() {}

#define ASSERT_OVERLAY(expected, app) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((app).overlay()))

static Course sampleCourse(int holes = 18) {
    Course c;
    std::strcpy(c.name, "Testplatz");
    c.hole_count = holes;
    for (int h = 0; h < kMaxHoles; h++) {
        c.holes[h].par = 4;
        c.holes[h].has_green = true;
        c.holes[h].green_center = offset(kTee, 300.0);
    }
    c.holes[2].has_green = false;
    return c;
}

// Dateisystem, auf dem bereits ein Platz liegt.
static void installCourse(MemoryFs& fs, int holes = 18) {
    Store store(fs);
    Course course = sampleCourse(holes);
    std::strcpy(course.id, "0123456789abcdef"); // wie ein vom Handy geladener Platz
    TEST_ASSERT_TRUE(store.saveCourse(course));
    fs.mutations = 0;
}

static std::unique_ptr<Session> start(MemoryFs& fs) { return std::unique_ptr<Session>(new Session(fs)); }

static void hit(Session& s, double metres_north) {
    s.app().setPosition(offset(kTee, metres_north));
    s.app().button1Short();
    s.sync();
    s.app().button1Short();
    s.sync();
}

static void finishHole(Session& s) {
    s.app().button2Short();
    s.sync();
    s.app().button2Short();
    s.sync();
}

// ---------- Start ----------

static void test_first_start_on_empty_storage() {
    MemoryFs fs;
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(18, s->app().round().holeCount());
    TEST_ASSERT_EQUAL_INT(0, s->app().currentHole());
    TEST_ASSERT_FALSE(s->app().course().holes[0].has_green);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(fs.files.size())); // Starten allein schreibt nichts
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_TRUE(fs.files.count("round.json") == 1);
    TEST_ASSERT_TRUE(fs.files.count("course.json") == 1);
}

static void test_stored_course_is_loaded() {
    MemoryFs fs;
    installCourse(fs, 9);
    auto s = start(fs);
    TEST_ASSERT_EQUAL_STRING("Testplatz", s->app().course().name);
    TEST_ASSERT_EQUAL_INT(9, s->app().round().holeCount());
    s->app().setPosition(kTee);
    TEST_ASSERT_EQUAL_INT(300, s->app().distances().center);
}

// ---------- Fortsetzen nach Neustart ----------

static void test_round_continues_after_restart() {
    MemoryFs fs;
    installCourse(fs);
    {
        auto s = start(fs);
        hit(*s, 0.0);
        hit(*s, 200.0);
        hit(*s, 300.0);
        finishHole(*s);
        hit(*s, 0.0);
    } // Geraet aus
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(1, s->app().currentHole());
    TEST_ASSERT_EQUAL_INT(3, s->app().round().shotCount(0));
    TEST_ASSERT_TRUE(s->app().round().isFinished(0));
    TEST_ASSERT_EQUAL_INT(1, s->app().round().putts(0));
    TEST_ASSERT_EQUAL_INT(1, s->app().round().shotCount(1));
    TEST_ASSERT_TRUE(s->app().round().shot(1, 0)->club == Club::Driver);
    ASSERT_OVERLAY(Overlay::None, s->app());
    // Schlagweiten stimmen weiterhin.
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 200.0, shotDistance(s->app().round(), s->app().course(), 0, 0, nullptr).meters);
}

static void test_restart_during_club_picker_reopens_it() {
    MemoryFs fs;
    installCourse(fs);
    {
        auto s = start(fs);
        s->app().setPosition(kTee);
        s->app().button1Short(); // Schlag gezaehlt, Schlaeger noch offen
        s->sync();
    }
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(1, s->app().round().shotCount(0));
    ASSERT_OVERLAY(Overlay::ClubPicker, s->app());
    s->app().button1Short();
    TEST_ASSERT_TRUE(s->app().round().shot(0, 0)->club == Club::Driver);
}

static void test_marked_green_survives_restart() {
    MemoryFs fs;
    installCourse(fs);
    {
        auto s = start(fs);
        for (int i = 0; i < 2; i++) {
            hit(*s, 0.0);
            finishHole(*s);
        }
        s->app().setPosition(offset(kTee, 420.0));
        s->app().button2Long();
        s->app().selectMenuItem(MenuItem::MarkGreen);
        TEST_ASSERT_TRUE(s->sync());
    }
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(2, s->app().currentHole());
    TEST_ASSERT_TRUE(s->app().course().holes[2].has_green);
    s->app().setPosition(kTee);
    TEST_ASSERT_EQUAL_INT(420, s->app().distances().center);
}

static void test_restart_without_sync_loses_only_the_unsynced_input() {
    MemoryFs fs;
    installCourse(fs);
    {
        auto s = start(fs);
        hit(*s, 0.0);
        s->app().setPosition(offset(kTee, 200.0));
        s->app().button1Short(); // nicht gesichert
    }
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(1, s->app().round().shotCount(0));
}

static void test_round_for_different_hole_count_is_not_restored() {
    MemoryFs fs;
    installCourse(fs, 18);
    {
        auto s = start(fs);
        hit(*s, 0.0);
    }
    Store(fs).saveCourse(sampleCourse(9)); // Platz wurde gegen einen 9-Loch-Platz getauscht
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(9, s->app().round().holeCount());
    TEST_ASSERT_EQUAL_INT(0, s->app().round().shotCount(0));
}

// ---------- Beschaedigte Daten ----------

static void test_corrupt_files_lead_to_clean_start() {
    MemoryFs fs;
    fs.files["course.json"] = "muell";
    fs.files["round.json"] = wrapPayload("{\"v\":1}");
    fs.files["round.json.new"] = "";
    auto s = start(fs);
    TEST_ASSERT_EQUAL_INT(18, s->app().round().holeCount());
    TEST_ASSERT_EQUAL_INT(0, s->app().round().shotCount(0));
    hit(*s, 0.0);
    TEST_ASSERT_TRUE(s->lastSyncOk());
    auto again = start(fs);
    TEST_ASSERT_EQUAL_INT(1, again->app().round().shotCount(0));
}

static void test_corrupt_round_keeps_valid_course() {
    MemoryFs fs;
    installCourse(fs);
    fs.files["round.json"] = "GOLF1 00000000 3\nabc";
    auto s = start(fs);
    TEST_ASSERT_EQUAL_STRING("Testplatz", s->app().course().name);
    TEST_ASSERT_EQUAL_INT(0, s->app().round().shotCount(0));
}

// ---------- Sparsames Schreiben ----------

static void test_sync_without_changes_writes_nothing() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    hit(*s, 0.0);
    const int before = fs.mutations;
    for (int i = 0; i < 100; i++) {
        s->app().setPosition(offset(kTee, i)); // Gehen aendert nichts Gespeichertes
        s->app().swipe(1);
        s->app().takeMessage();
        TEST_ASSERT_TRUE(s->sync());
    }
    TEST_ASSERT_EQUAL_INT(before, fs.mutations);
}

static void test_restart_without_changes_writes_nothing() {
    MemoryFs fs;
    installCourse(fs);
    {
        auto s = start(fs);
        hit(*s, 0.0);
    }
    const int before = fs.mutations;
    auto s = start(fs);
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_EQUAL_INT(before, fs.mutations);
}

static void test_course_is_only_written_when_it_changes() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    const std::string course_before = fs.files["course.json"];
    hit(*s, 0.0);
    hit(*s, 100.0);
    TEST_ASSERT_TRUE(course_before == fs.files["course.json"]);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(fs.files.count("course.json.new")));
}

static void test_course_file_without_id_gets_one_and_is_saved_once() {
    // Platzdatei aus einer frueheren Firmware.
    MemoryFs fs;
    Store(fs).saveCourse(sampleCourse());
    std::string id;
    {
        auto s = start(fs);
        TEST_ASSERT_TRUE(isValidId(s->app().course().id));
        id = s->app().course().id;
        TEST_ASSERT_TRUE(s->sync());
    }
    const int writes = fs.mutations;
    auto s = start(fs);
    TEST_ASSERT_EQUAL_STRING(id.c_str(), s->app().course().id); // dieselbe, nicht jedes Mal neu
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_EQUAL_INT(writes, fs.mutations);
}

// ---------- Speicherfehler ----------

static void test_failed_save_is_reported_and_retried() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    hit(*s, 0.0);
    fs.read_only = true; // Karte entfernt
    s->app().setPosition(offset(kTee, 200.0));
    s->app().button1Short();
    TEST_ASSERT_FALSE(s->sync());
    TEST_ASSERT_FALSE(s->lastSyncOk());
    s->app().button1Short();
    TEST_ASSERT_FALSE(s->sync());
    TEST_ASSERT_EQUAL_INT(2, s->app().round().shotCount(0)); // die App laeuft weiter

    fs.read_only = false; // Karte wieder da
    TEST_ASSERT_TRUE(s->sync()); // ohne neue Eingabe wird nachgeholt
    TEST_ASSERT_TRUE(s->lastSyncOk());
    auto again = start(fs);
    TEST_ASSERT_EQUAL_INT(2, again->app().round().shotCount(0));
}

// ---------- Stromausfall ----------

static void test_power_cut_at_every_point_of_a_whole_hole() {
    // Ein Loch spielen und dabei bei jeder einzelnen Schreiboperation einmal
    // den Strom ausfallen lassen. Nach dem Neustart darf hoechstens die letzte
    // Eingabe fehlen, und der Stand muss in sich stimmig sein.
    int total_ops = 0;
    {
        MemoryFs fs;
        installCourse(fs);
        auto s = start(fs);
        hit(*s, 0.0);
        hit(*s, 200.0);
        hit(*s, 300.0);
        finishHole(*s);
        total_ops = fs.mutations;
    }
    TEST_ASSERT_TRUE(total_ops > 20);

    for (int cut = 1; cut <= total_ops; cut++) {
        for (int variant = 0; variant < 4; variant++) {
            MemoryFs fs;
            installCourse(fs);
            fs.power_cut_at = cut;
            fs.partial_write = (variant & 1) ? 0.6 : 0.0;
            fs.cut_op_completes = (variant & 2) != 0;

            // Dieselben Eingaben; nach jeder wird gesichert. Wie viele Schlaege
            // waren vor dem Ausfall bestaetigt gesichert?
            int confirmed_shots = 0;
            int attempted_shots = 0;
            bool hole_done_attempted = false;
            {
                auto s = start(fs);
                const double positions[] = {0.0, 200.0, 300.0};
                for (double p : positions) {
                    if (fs.dead) break;
                    s->app().setPosition(offset(kTee, p));
                    s->app().button1Short();
                    attempted_shots = s->app().round().shotCount(0);
                    if (s->sync()) confirmed_shots = attempted_shots;
                    if (fs.dead) break;
                    s->app().button1Short();
                    s->sync();
                }
                if (!fs.dead) {
                    hole_done_attempted = true;
                    finishHole(*s);
                }
            }
            fs.reboot();

            auto s = start(fs);
            const int shots = s->app().round().shotCount(0);
            char msg[80];
            std::snprintf(msg, sizeof(msg), "Ausfall bei Op %d, Variante %d: %d Schlaege", cut, variant, shots);
            TEST_ASSERT_TRUE_MESSAGE(shots >= confirmed_shots, msg); // nichts Bestaetigtes verloren
            TEST_ASSERT_TRUE_MESSAGE(shots <= attempted_shots, msg); // nichts erfunden
            TEST_ASSERT_EQUAL_STRING_MESSAGE("Testplatz", s->app().course().name, msg);
            if (!hole_done_attempted) TEST_ASSERT_EQUAL_INT_MESSAGE(0, s->app().currentHole(), msg);
            for (int i = 0; i < shots - 1; i++) {
                TEST_ASSERT_TRUE_MESSAGE(s->app().round().shot(0, i)->club != Club::None, msg);
            }
            // Weiterspielen und Sichern funktioniert danach wieder.
            s->app().button1Long();
            TEST_ASSERT_TRUE_MESSAGE(s->sync(), msg);
        }
    }
}

// ---------- Rundenende und Archiv ----------

static void endRoundAndStartNew(Session& s) {
    s.app().button2Long();
    s.app().selectMenuItem(MenuItem::EndRound);
    s.sync();
    s.app().button1Long();
    s.sync();
}

static void test_new_round_archives_the_old_one() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    hit(*s, 0.0);
    hit(*s, 300.0);
    finishHole(*s);
    endRoundAndStartNew(*s);

    const std::vector<int> list = s->store().archivedRounds();
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(list.size()));
    std::unique_ptr<RoundState> archived(new RoundState());
    TEST_ASSERT_TRUE(s->store().loadArchivedRound(1, *archived));
    TEST_ASSERT_EQUAL_INT(2, archived->round.shotCount(0));
    TEST_ASSERT_TRUE(archived->round.isFinished(0));
    TEST_ASSERT_EQUAL_STRING("Testplatz", archived->course_name);

    // Die neue Runde ist leer und bleibt es auch nach einem Neustart.
    auto again = start(fs);
    TEST_ASSERT_EQUAL_INT(0, again->app().currentHole());
    TEST_ASSERT_EQUAL_INT(0, again->app().round().shotCount(0));
}

static void test_empty_round_is_not_archived() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    endRoundAndStartNew(*s);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(s->store().archivedRounds().size()));
}

static void test_several_rounds_are_archived_in_order() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    for (int round = 1; round <= 3; round++) {
        for (int i = 0; i < round; i++) hit(*s, i * 50.0);
        endRoundAndStartNew(*s);
    }
    TEST_ASSERT_EQUAL_INT(3, static_cast<int>(s->store().archivedRounds().size()));
    std::unique_ptr<RoundState> archived(new RoundState());
    for (int round = 1; round <= 3; round++) {
        TEST_ASSERT_TRUE(s->store().loadArchivedRound(round, *archived));
        TEST_ASSERT_EQUAL_INT(round, archived->round.shotCount(0));
    }
}

static void test_archiving_is_retried_when_storage_fails() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    hit(*s, 0.0);
    s->app().button2Long();
    s->app().selectMenuItem(MenuItem::EndRound);
    s->sync();
    fs.read_only = true;
    s->app().button1Long(); // neue Runde, aber Archivieren scheitert
    TEST_ASSERT_FALSE(s->sync());
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(s->store().archivedRounds().size()));
    hit(*s, 0.0); // es wird weitergespielt
    fs.read_only = false;
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(s->store().archivedRounds().size()));
    std::unique_ptr<RoundState> archived(new RoundState());
    TEST_ASSERT_TRUE(s->store().loadArchivedRound(1, *archived));
    TEST_ASSERT_EQUAL_INT(1, archived->round.shotCount(0));
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(s->store().archivedRounds().size())); // nicht doppelt
}

// ---------- Startzeit ----------

static DateTime at(int hour, int minute) {
    DateTime d;
    d.valid = true;
    d.year = 2026;
    d.month = 11;
    d.day = 23;
    d.hour = hour;
    d.minute = minute;
    return d;
}

static void test_round_start_time_is_taken_at_first_shot() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    s->setDateTime(at(8, 0));
    s->sync();
    TEST_ASSERT_FALSE(s->roundStarted().valid); // noch kein Schlag
    s->setDateTime(at(8, 15));
    hit(*s, 0.0);
    s->setDateTime(at(9, 30));
    hit(*s, 200.0);
    TEST_ASSERT_TRUE(s->roundStarted().valid);
    TEST_ASSERT_EQUAL_INT(8, s->roundStarted().hour);
    TEST_ASSERT_EQUAL_INT(15, s->roundStarted().minute);

    auto again = start(fs);
    TEST_ASSERT_EQUAL_INT(15, again->roundStarted().minute); // uebersteht den Neustart
}

static void test_start_time_goes_into_archive_and_resets() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    s->setDateTime(at(8, 15));
    hit(*s, 0.0);
    s->setDateTime(at(13, 40));
    endRoundAndStartNew(*s);
    TEST_ASSERT_FALSE(s->roundStarted().valid);
    hit(*s, 0.0);
    TEST_ASSERT_EQUAL_INT(13, s->roundStarted().hour);

    std::unique_ptr<RoundState> archived(new RoundState());
    TEST_ASSERT_TRUE(s->store().loadArchivedRound(1, *archived));
    TEST_ASSERT_TRUE(archived->started.valid);
    TEST_ASSERT_EQUAL_INT(8, archived->started.hour);
}

static void test_start_time_is_added_later_if_gps_time_arrives_late() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    hit(*s, 0.0); // noch keine Uhrzeit vom GPS
    TEST_ASSERT_FALSE(s->roundStarted().valid);
    s->setDateTime(at(8, 20));
    TEST_ASSERT_TRUE(s->sync());
    TEST_ASSERT_TRUE(s->roundStarted().valid);
    auto again = start(fs);
    TEST_ASSERT_EQUAL_INT(20, again->roundStarted().minute);
}

// ---------- Zufaellige Bedienung mit Neustarts ----------

static void test_random_play_with_restarts_always_restores_last_synced_state() {
    MemoryFs fs;
    installCourse(fs);
    auto s = start(fs);
    uint32_t state = 99;
    for (int i = 0; i < 4000; i++) {
        state = state * 1664525u + 1013904223u;
        const uint32_t r = state >> 8;
        switch (r % 12) {
            case 0:
            case 1:
            case 2: s->app().button1Short(); break;
            case 3: s->app().button1Long(); break;
            case 4:
            case 5: s->app().button2Short(); break;
            case 6: s->app().button2Long(); break;
            case 7: s->app().selectClub(static_cast<Club>((r >> 4) % kClubCount)); break;
            case 8: s->app().selectMenuItem(static_cast<MenuItem>((r >> 4) % kMenuItemCount)); break;
            case 9: s->app().setPosition(offset(kTee, static_cast<double>((r >> 4) % 400))); break;
            case 10: s->app().clearPosition(); break;
            case 11: s->app().tap(); break;
        }
        TEST_ASSERT_TRUE(s->sync());

        if (i % 37 == 0) {
            const std::string expected_round =
                roundToJson(s->app().round(), s->app().currentHole(), s->roundStarted(), s->app().course().name);
            const std::string expected_course = courseToJson(s->app().course());
            s = start(fs);
            TEST_ASSERT_TRUE(expected_round ==
                             roundToJson(s->app().round(), s->app().currentHole(), s->roundStarted(), s->app().course().name));
            TEST_ASSERT_TRUE(expected_course == courseToJson(s->app().course()));
        }
    }
    // Keine liegengebliebenen Hilfsdateien.
    for (const auto& entry : fs.files) {
        TEST_ASSERT_TRUE_MESSAGE(entry.first.find(".new") == std::string::npos, entry.first.c_str());
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_start_on_empty_storage);
    RUN_TEST(test_stored_course_is_loaded);
    RUN_TEST(test_round_continues_after_restart);
    RUN_TEST(test_restart_during_club_picker_reopens_it);
    RUN_TEST(test_marked_green_survives_restart);
    RUN_TEST(test_restart_without_sync_loses_only_the_unsynced_input);
    RUN_TEST(test_round_for_different_hole_count_is_not_restored);
    RUN_TEST(test_corrupt_files_lead_to_clean_start);
    RUN_TEST(test_corrupt_round_keeps_valid_course);
    RUN_TEST(test_sync_without_changes_writes_nothing);
    RUN_TEST(test_restart_without_changes_writes_nothing);
    RUN_TEST(test_course_is_only_written_when_it_changes);
    RUN_TEST(test_course_file_without_id_gets_one_and_is_saved_once);
    RUN_TEST(test_failed_save_is_reported_and_retried);
    RUN_TEST(test_power_cut_at_every_point_of_a_whole_hole);
    RUN_TEST(test_new_round_archives_the_old_one);
    RUN_TEST(test_empty_round_is_not_archived);
    RUN_TEST(test_several_rounds_are_archived_in_order);
    RUN_TEST(test_archiving_is_retried_when_storage_fails);
    RUN_TEST(test_round_start_time_is_taken_at_first_shot);
    RUN_TEST(test_start_time_goes_into_archive_and_resets);
    RUN_TEST(test_start_time_is_added_later_if_gps_time_arrives_late);
    RUN_TEST(test_random_play_with_restarts_always_restores_last_synced_state);
    return UNITY_END();
}
