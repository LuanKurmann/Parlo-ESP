#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include <unity.h>

#include "../memory_fs.h"
#include "../test_support.h"
#include "core/demo_course.h"
#include "core/gps_feed.h"
#include "core/nmea_log.h"
#include "core/nmea_writer.h"

using namespace golf;
using testsupport::kTee;
using testsupport::MemoryFs;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static NmeaFixInfo at(const LatLon& position, int second = 0) {
    NmeaFixInfo info;
    info.position = position;
    info.hour = 8 + second / 3600;
    info.minute = (second / 60) % 60;
    info.second = second % 60;
    return info;
}

static NmeaFixInfo noFix(int second = 0) {
    NmeaFixInfo info = at(kTee, second);
    info.fix = false;
    return info;
}

struct Rig {
    MemoryFs fs;
    std::unique_ptr<Session> session;
    std::unique_ptr<GpsFeed> feed;

    Rig() {
        Store(fs).saveCourse(demoCourse());
        session.reset(new Session(fs));
        feed.reset(new GpsFeed(*session));
    }
    App& app() { return session->app(); }
    void send(const std::string& text, uint32_t now_ms) { feed->feed(text.data(), text.size(), now_ms); }
};

// ---------- Erzeugte Saetze ----------

static void test_writer_produces_valid_checksums() {
    TEST_ASSERT_EQUAL_STRING("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",
                             nmeaSentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,").c_str());
}

static void test_writer_and_parser_agree_on_positions() {
    // Rund um die Welt, inklusive Faellen, in denen die Minuten auf 60 runden wuerden.
    const LatLon positions[] = {{47.3769, 8.5417},   {-33.8915, 151.2020},       {56.3431, -2.8025}, {0.0, 0.0},
                                {-0.00001, -0.00001}, {47.99999999, 8.99999999},  {89.9999, 179.9999}, {-89.9999, -179.9999},
                                {12.0166666667, 100.9833333333}};
    for (const LatLon& p : positions) {
        NmeaParser gga, rmc;
        const std::string a = nmeaGga(at(p)), b = nmeaRmc(at(p));
        gga.feed(a.data(), a.size());
        rmc.feed(b.data(), b.size());
        char msg[64];
        std::snprintf(msg, sizeof(msg), "%.8f %.8f", p.lat, p.lon);
        TEST_ASSERT_TRUE_MESSAGE(gga.fix().valid, msg);
        TEST_ASSERT_TRUE_MESSAGE(rmc.fix().valid, msg);
        TEST_ASSERT_TRUE_MESSAGE(distanceMeters(p, gga.fix().position) < 0.02, msg); // 2 cm
        TEST_ASSERT_TRUE_MESSAGE(distanceMeters(p, rmc.fix().position) < 0.02, msg);
    }
}

static void test_writer_carries_time_date_and_quality() {
    NmeaFixInfo info = at(kTee, 3725); // 09:02:05
    info.satellites = 9;
    info.hdop = 1.35;
    info.altitude_m = 512.4;
    info.speed_kmh = 5.0;
    NmeaParser p;
    const std::string burst = nmeaBurst(info);
    p.feed(burst.data(), burst.size());
    TEST_ASSERT_EQUAL_INT(9, p.fix().hour);
    TEST_ASSERT_EQUAL_INT(2, p.fix().minute);
    TEST_ASSERT_EQUAL_INT(5, p.fix().second);
    TEST_ASSERT_EQUAL_INT(2026, p.fix().year);
    TEST_ASSERT_EQUAL_INT(11, p.fix().month);
    TEST_ASSERT_EQUAL_INT(24, p.fix().day);
    TEST_ASSERT_EQUAL_INT(9, p.fix().satellites);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.35f, p.fix().hdop);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 512.4f, p.fix().altitude_m);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, p.fix().speed_kmh);
    TEST_ASSERT_EQUAL_UINT32(4, p.acceptedSentences());
    TEST_ASSERT_EQUAL_UINT32(0, p.rejectedSentences());
}

static void test_writer_without_fix_and_without_time() {
    NmeaParser p;
    std::string burst = nmeaBurst(noFix(5));
    p.feed(burst.data(), burst.size());
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_TRUE(p.fix().has_time);
    TEST_ASSERT_EQUAL_UINT32(0, p.rejectedSentences());

    NmeaFixInfo cold = noFix();
    cold.has_time = false;
    NmeaParser q;
    burst = nmeaBurst(cold);
    q.feed(burst.data(), burst.size());
    TEST_ASSERT_FALSE(q.fix().valid);
    TEST_ASSERT_FALSE(q.fix().has_time);
    TEST_ASSERT_FALSE(q.fix().has_date);
    TEST_ASSERT_EQUAL_UINT32(0, q.rejectedSentences());
}

// ---------- Position in die App ----------

static void test_position_reaches_the_app() {
    Rig rig;
    TEST_ASSERT_FALSE(rig.app().hasPosition());
    rig.send(nmeaBurst(at(rig.app().course().holes[0].tee)), 1000);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(342, rig.app().distances().center);
    // Gut zwei Minuten spaeter 200 m weiter (normales Gehtempo).
    rig.send(nmeaBurst(at(positionOnHole(rig.app().course().holes[0], 200.0), 150)), 151000);
    TEST_ASSERT_EQUAL_INT(142, rig.app().distances().center);
}

static void test_receiver_reporting_no_fix_clears_position() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee)), 1000);
    rig.send(nmeaBurst(noFix(1)), 2000);
    TEST_ASSERT_FALSE(rig.app().hasPosition());
    rig.send(nmeaBurst(at(kTee, 2)), 3000);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
}

static void test_silent_receiver_times_out() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee)), 1000);
    rig.feed->tick(1000 + GpsFeed::kTimeoutMs - 1);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    rig.feed->tick(1000 + GpsFeed::kTimeoutMs);
    TEST_ASSERT_FALSE(rig.app().hasPosition());
    rig.feed->tick(60000); // bleibt weg, nichts stuerzt ab
    TEST_ASSERT_FALSE(rig.app().hasPosition());
    rig.send(nmeaBurst(at(kTee, 60)), 61000);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
}

static void test_no_timeout_while_fixes_keep_coming() {
    Rig rig;
    for (int s = 0; s < 600; s++) {
        rig.send(nmeaBurst(at(kTee, s)), 1000u * static_cast<uint32_t>(s));
        for (uint32_t ms = 0; ms < 1000; ms += 50) rig.feed->tick(1000u * static_cast<uint32_t>(s) + ms);
        TEST_ASSERT_TRUE(rig.app().hasPosition());
    }
}

static void test_garbage_only_times_out_too() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee)), 0);
    for (uint32_t t = 1000; t <= 8000; t += 1000) {
        rig.send("$GNGGA,kaputt*00\r\nrauschen\r\n", t);
        rig.feed->tick(t);
    }
    TEST_ASSERT_FALSE(rig.app().hasPosition());
}

static void test_timeout_works_across_timer_overflow() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee)), 0xFFFFF000u);
    rig.feed->tick(0xFFFFFF00u);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    rig.feed->tick(0x00000100u); // nur rund 4 Sekunden spaeter
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    rig.feed->tick(0x00001000u); // rund 8 Sekunden
    TEST_ASSERT_FALSE(rig.app().hasPosition());
}

// ---------- Unplausible Spruenge ----------

static void test_single_wild_jump_is_ignored() {
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    rig.send(nmeaBurst(at(positionOnHole(hole, 100.0), 0)), 1000);
    rig.send(nmeaBurst(at(LatLon{48.3769, 8.5417}, 1)), 2000); // 111 km weiter noerdlich
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(242, rig.app().distances().center); // alte Position gilt weiter
    TEST_ASSERT_EQUAL_UINT32(2, rig.feed->rejectedJumps());   // GGA und RMC
    rig.send(nmeaBurst(at(positionOnHole(hole, 101.0), 2)), 3000);
    TEST_ASSERT_EQUAL_INT(241, rig.app().distances().center);
}

static void test_normal_movement_is_never_rejected() {
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    // Gehen (1.4 m/s), Golfcart (7 m/s) und Stehen mit 5 m GPS-Springen.
    double along = 0.0;
    for (int s = 0; s < 300; s++) {
        along += s < 100 ? 1.4 : (s < 200 ? 7.0 : ((s % 2) ? 5.0 : -5.0));
        rig.send(nmeaBurst(at(positionOnHole(hole, along), s)), 1000u * static_cast<uint32_t>(s + 1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, rig.feed->rejectedJumps());
}

static void test_real_relocation_is_accepted_after_a_few_fixes() {
    // Geraet war aus dem Empfang (Auto, Tasche) und taucht woanders auf, ohne
    // dass der Empfaenger zwischendurch "kein Empfang" gemeldet hat.
    Rig rig;
    const LatLon far{47.5, 8.7};
    rig.send(nmeaBurst(at(kTee, 0)), 1000);
    rig.send(nmeaGga(at(far, 1)), 2000);
    rig.send(nmeaGga(at(far, 2)), 3000);
    rig.send(nmeaGga(at(far, 3)), 4000);
    TEST_ASSERT_EQUAL_INT(342, rig.app().distances().center); // noch die alte
    rig.send(nmeaGga(at(far, 4)), 5000);
    TEST_ASSERT_TRUE(rig.app().distances().center > 900); // jetzt die neue
}

static void test_jump_after_reported_loss_is_accepted_immediately() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee, 0)), 1000);
    rig.send(nmeaBurst(noFix(1)), 2000);
    rig.send(nmeaBurst(at(LatLon{47.5, 8.7}, 2)), 3000);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    TEST_ASSERT_EQUAL_UINT32(0, rig.feed->rejectedJumps());
}

static void test_jump_after_timeout_is_accepted_immediately() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee, 0)), 1000);
    rig.feed->tick(20000);
    rig.send(nmeaBurst(at(LatLon{47.5, 8.7}, 20)), 21000);
    TEST_ASSERT_TRUE(rig.app().hasPosition());
    TEST_ASSERT_EQUAL_UINT32(0, rig.feed->rejectedJumps());
}

static void test_small_outlier_is_removed_by_the_median() {
    // 20 m daneben: langsam genug, um nicht als Sprung zu gelten.
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    rig.send(nmeaBurst(at(positionOnHole(hole, 100.0), 0)), 1000);
    rig.send(nmeaGga(at(positionOnHole(hole, 120.0), 1)), 2000); // verfaelschter Satz
    TEST_ASSERT_EQUAL_UINT32(0, rig.feed->rejectedJumps());
    TEST_ASSERT_EQUAL_INT(242, rig.app().distances().center);
    rig.send(nmeaRmc(at(positionOnHole(hole, 101.4), 1)), 2000); // der richtige derselben Sekunde
    TEST_ASSERT_EQUAL_INT(241, rig.app().distances().center);
    rig.send(nmeaBurst(at(positionOnHole(hole, 102.8), 2)), 3000);
    TEST_ASSERT_EQUAL_INT(239, rig.app().distances().center);
}

static void test_median_adds_no_lag_while_walking() {
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    for (int s = 0; s < 60; s++) {
        const double along = s * 1.4;
        rig.send(nmeaBurst(at(positionOnHole(hole, along), s)), 1000u * static_cast<uint32_t>(s + 1));
        TEST_ASSERT_INT_WITHIN(1, static_cast<int>(std::lround(342.0 - along)), rig.app().distances().center);
    }
}

static void test_median_works_with_a_receiver_sending_only_gga() {
    // Dann liegt die Anzeige hoechstens eine Sekunde zurueck.
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    for (int s = 0; s < 30; s++) {
        rig.send(nmeaGga(at(positionOnHole(hole, s * 1.4), s)), 1000u * static_cast<uint32_t>(s + 1));
    }
    TEST_ASSERT_INT_WITHIN(2, static_cast<int>(std::lround(342.0 - 29 * 1.4)), rig.app().distances().center);
}

static void test_old_samples_do_not_hold_the_position_back() {
    Rig rig;
    const Hole& hole = rig.app().course().holes[0];
    rig.send(nmeaBurst(at(positionOnHole(hole, 0.0), 0)), 1000);
    // 4 Sekunden lang kommt nichts Brauchbares, dann ein einzelner Satz 5 m weiter.
    rig.send(nmeaGga(at(positionOnHole(hole, 5.0), 4)), 5000);
    TEST_ASSERT_EQUAL_INT(337, rig.app().distances().center);
}

// ---------- Uhrzeit ----------

static void test_gps_time_becomes_round_start() {
    Rig rig;
    rig.send(nmeaBurst(at(kTee, 15 * 60 + 30)), 1000); // 08:15:30
    rig.app().button1Short();
    rig.app().button1Short();
    rig.session->sync();
    TEST_ASSERT_TRUE(rig.session->roundStarted().valid);
    TEST_ASSERT_EQUAL_INT(2026, rig.session->roundStarted().year);
    TEST_ASSERT_EQUAL_INT(8, rig.session->roundStarted().hour);
    TEST_ASSERT_EQUAL_INT(15, rig.session->roundStarted().minute);
}

static void test_time_without_date_is_not_used() {
    Rig rig;
    rig.send(nmeaGga(at(kTee, 60)), 1000); // GGA kennt nur die Uhrzeit
    rig.app().button1Short();
    rig.app().button1Short();
    rig.session->sync();
    TEST_ASSERT_FALSE(rig.session->roundStarted().valid);
}

// ---------- Aufzeichnung abspielen ----------

static void test_log_groups_sentences_by_second() {
    std::string text;
    for (int s = 0; s < 5; s++) text += nmeaBurst(at(offset(kTee, s * 1.4), s));
    NmeaLog log(text);
    TEST_ASSERT_EQUAL_UINT32(5, log.burstCount());
    std::string burst;
    int count = 0;
    while (log.next(burst)) {
        NmeaParser p;
        p.feed(burst.data(), burst.size());
        TEST_ASSERT_EQUAL_UINT32(4, p.acceptedSentences());
        TEST_ASSERT_EQUAL_INT(count, p.fix().second);
        count++;
    }
    TEST_ASSERT_EQUAL_INT(5, count);
    TEST_ASSERT_FALSE(log.next(burst));
    log.rewind();
    TEST_ASSERT_TRUE(log.next(burst));
    TEST_ASSERT_EQUAL_UINT32(1, log.position());
}

static void test_log_tolerates_messy_files() {
    const std::string text = "# Kommentar\n\n" + nmeaGga(at(kTee, 0)) + "irgendwas dazwischen\n" + nmeaRmc(at(kTee, 0)) +
                             "   \n$GNGGA,kaputt\n" + nmeaGga(at(kTee, 1)).substr(0, 70) + "\n" + // ohne CR, abgeschnitten
                             nmeaRmc(at(kTee, 2));
    NmeaLog log(text);
    TEST_ASSERT_TRUE(log.burstCount() >= 3);
    std::string burst;
    NmeaParser p;
    while (log.next(burst)) p.feed(burst.data(), burst.size());
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_EQUAL_INT(2, p.fix().second);
}

static void test_log_of_empty_or_foreign_file() {
    std::string burst;
    NmeaLog empty("");
    TEST_ASSERT_EQUAL_UINT32(0, empty.burstCount());
    TEST_ASSERT_FALSE(empty.next(burst));
    NmeaLog foreign("<?xml version=\"1.0\"?>\n<gpx></gpx>\n");
    TEST_ASSERT_EQUAL_UINT32(0, foreign.burstCount());
}

static void test_log_without_position_sentences_is_one_burst() {
    NmeaLog log(nmeaSentence("GPGSV,1,1,00") + nmeaSentence("GPGSV,1,1,00"));
    TEST_ASSERT_EQUAL_UINT32(1, log.burstCount());
}

// ---------- Beispielplatz ----------

static void test_demo_course_is_complete() {
    const Course c = demoCourse();
    int par = 0;
    for (int h = 0; h < 18; h++) {
        par += c.holes[h].par;
        TEST_ASSERT_TRUE(c.holes[h].has_green && c.holes[h].has_tee);
        TEST_ASSERT_TRUE(holeLengthMeters(c.holes[h]) > 100);
    }
    TEST_ASSERT_EQUAL_INT(72, par);
    TEST_ASSERT_EQUAL_INT(342, holeLengthMeters(c.holes[0]));
}

static void test_position_on_hole_along_and_sideways() {
    const Hole hole = demoCourse().holes[0];
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 100.0, distanceMeters(hole.tee, positionOnHole(hole, 100.0)));
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 242.0, distanceMeters(hole.green_center, positionOnHole(hole, 100.0)));
    TEST_ASSERT_DOUBLE_WITHIN(0.2, 20.0, distanceMeters(positionOnHole(hole, 100.0), positionOnHole(hole, 100.0, 20.0)));
    TEST_ASSERT_DOUBLE_WITHIN(0.3, 40.0, distanceMeters(positionOnHole(hole, 100.0, -20.0), positionOnHole(hole, 100.0, 20.0)));
    TEST_ASSERT_TRUE(positionOnHole(hole, 100.0, 20.0).lon > hole.tee.lon); // rechts ist bei Nordrichtung Osten
    // Der seitliche Versatz veraendert die Distanz entlang der Bahn kaum.
    TEST_ASSERT_DOUBLE_WITHIN(0.2, std::sqrt(100.0 * 100.0 + 20.0 * 20.0), distanceMeters(hole.tee, positionOnHole(hole, 100.0, 20.0)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_writer_produces_valid_checksums);
    RUN_TEST(test_writer_and_parser_agree_on_positions);
    RUN_TEST(test_writer_carries_time_date_and_quality);
    RUN_TEST(test_writer_without_fix_and_without_time);
    RUN_TEST(test_position_reaches_the_app);
    RUN_TEST(test_receiver_reporting_no_fix_clears_position);
    RUN_TEST(test_silent_receiver_times_out);
    RUN_TEST(test_no_timeout_while_fixes_keep_coming);
    RUN_TEST(test_garbage_only_times_out_too);
    RUN_TEST(test_timeout_works_across_timer_overflow);
    RUN_TEST(test_single_wild_jump_is_ignored);
    RUN_TEST(test_normal_movement_is_never_rejected);
    RUN_TEST(test_real_relocation_is_accepted_after_a_few_fixes);
    RUN_TEST(test_jump_after_reported_loss_is_accepted_immediately);
    RUN_TEST(test_jump_after_timeout_is_accepted_immediately);
    RUN_TEST(test_small_outlier_is_removed_by_the_median);
    RUN_TEST(test_median_adds_no_lag_while_walking);
    RUN_TEST(test_median_works_with_a_receiver_sending_only_gga);
    RUN_TEST(test_old_samples_do_not_hold_the_position_back);
    RUN_TEST(test_gps_time_becomes_round_start);
    RUN_TEST(test_time_without_date_is_not_used);
    RUN_TEST(test_log_groups_sentences_by_second);
    RUN_TEST(test_log_tolerates_messy_files);
    RUN_TEST(test_log_of_empty_or_foreign_file);
    RUN_TEST(test_log_without_position_sentences_is_one_burst);
    RUN_TEST(test_demo_course_is_complete);
    RUN_TEST(test_position_on_hole_along_and_sideways);
    return UNITY_END();
}
