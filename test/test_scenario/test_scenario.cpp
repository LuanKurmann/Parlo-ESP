#include <cstdio>
#include <cstring>
#include <memory>

#include <unity.h>

#include "../scenario.h"
#include "core/display_text.h"

using namespace golf;
using testsupport::MemoryFs;
using testsupport::Scenario;

void setUp() {}
void tearDown() {}

#define ASSERT_OVERLAY(expected, app) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((app).overlay()))

static void installDemoCourse(MemoryFs& fs) { TEST_ASSERT_TRUE(Store(fs).saveCourse(demoCourse())); }

// Am Ball: kurz stehen, Knopf 1 (Schlag), Knopf 1 (Vorschlag bestaetigen).
static void shotHere(Scenario& s) {
    s.wait(4.0);
    s.press1();
    s.wait(1.0);
    s.press1();
    s.wait(1.0);
}

// Ein Loch nach Plan: Schlaege an den angegebenen Stellen (Meter ab Abschlag,
// Meter seitlich), danach `putts` Putts auf dem Gruen und Loch abschliessen.
struct Spot {
    double along;
    double right;
};

static void playHole(Scenario& s, int hole_index, const Spot* spots, int spot_count, int putts) {
    const Hole hole = s.app().course().holes[hole_index];
    const double length = distanceMeters(hole.tee, hole.green_center);
    s.walkTo(hole.tee);
    for (int i = 0; i < spot_count; i++) {
        s.walkTo(positionOnHole(hole, spots[i].along, spots[i].right));
        shotHere(s);
    }
    for (int i = 0; i < putts; i++) {
        s.walkTo(positionOnHole(hole, length - 6.0 + i * 5.0, 1.0)); // 6 m und 1 m vor dem Loch
        shotHere(s);
    }
    s.walkTo(hole.green_center);
    s.wait(2.0);
    s.press2(); // Loch abschliessen
    s.wait(2.0);
    s.press2(); // weiter
    s.wait(1.0);
}

// ---------- Ein Loch im Detail ----------

static void test_one_hole_from_bytes_and_button_levels() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Hole hole = s.app().course().holes[0]; // Par 4, 342 m

    s.placeAt(hole.tee);
    s.wait(3.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(342, s.app().distances().center);

    s.press1(); // Abschlag
    ASSERT_OVERLAY(Overlay::ClubPicker, s.app());
    TEST_ASSERT_TRUE(s.app().suggestedClub() == Club::Driver);
    s.press1();
    ASSERT_OVERLAY(Overlay::None, s.app());

    s.walkTo(positionOnHole(hole, 205.0, 12.0)); // Drive 205 m, leicht rechts
    s.wait(2.0); // am Ball stehen: der Empfaenger meldet nur einmal pro Sekunde
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 205 m", distanceScreenText(s.app()).foot);
    TEST_ASSERT_EQUAL_INT(138, s.app().distances().center); // schraeg zum Gruen etwas weiter als 137
    s.press1();
    TEST_ASSERT_TRUE(s.app().suggestedClub() == Club::Iron7);
    s.press2(); // lieber Eisen 8
    s.press1();
    TEST_ASSERT_TRUE(s.app().round().shot(0, 1)->club == Club::Iron8);

    s.walkTo(positionOnHole(hole, 336.0)); // auf dem Gruen, 6 m vor der Fahne
    s.press1();
    TEST_ASSERT_TRUE(s.app().suggestedClub() == Club::Putter);
    s.press1();
    s.walkTo(positionOnHole(hole, 341.0));
    s.press1();
    s.press1();

    s.press2();
    ASSERT_OVERLAY(Overlay::HoleDone, s.app());
    const HoleDoneText done = holeDoneText(s.app());
    TEST_ASSERT_EQUAL_STRING("4", done.strokes);
    TEST_ASSERT_EQUAL_STRING("Par", done.name);
    TEST_ASSERT_EQUAL_STRING("2 Putts", done.putts);
    s.press2();
    TEST_ASSERT_EQUAL_INT(1, s.app().currentHole());

    const ShotDistance drive = shotDistance(s.app().round(), s.app().course(), 0, 0, nullptr);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 205.35, drive.meters); // Wurzel aus 205^2 + 12^2
    TEST_ASSERT_EQUAL_INT(0, s.failedSyncs());
}

static void test_long_press_undoes_and_opens_menu() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    s.placeAt(s.app().course().holes[0].tee);
    s.wait(2.0);
    s.press1();
    s.press1();
    s.press1(); // versehentlich ein zweiter Schlag
    s.hold1();  // lang: rueckgaengig
    TEST_ASSERT_EQUAL_INT(1, s.app().round().shotCount(0));
    ASSERT_OVERLAY(Overlay::None, s.app());
    TEST_ASSERT_TRUE(s.app().takeMessage() == Message::ShotRemoved);

    s.hold2(); // lang: Menue
    ASSERT_OVERLAY(Overlay::Menu, s.app());
    s.press2();
    s.press2();
    s.press1(); // "Runde beenden"
    ASSERT_OVERLAY(Overlay::RoundSummary, s.app());
    s.press1(); // kurz tut nichts
    ASSERT_OVERLAY(Overlay::RoundSummary, s.app());
    s.hold1(); // lang: neue Runde
    ASSERT_OVERLAY(Overlay::None, s.app());
    TEST_ASSERT_EQUAL_INT(0, s.app().round().shotCount(0));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(s.session().store().archivedRounds().size()));
}

// ---------- Ganze Runde ----------

static void test_full_round_with_realistic_gps_noise() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs, 2026);
    s.noise_m = 1.5; // typische Streuung eines guten Empfaengers unter freiem Himmel

    int expected_strokes = 0, expected_putts = 0;
    for (int h = 0; h < 18; h++) {
        const Hole hole = s.app().course().holes[h];
        const double length = holeLengthMeters(hole);
        const int putts = 1 + (h % 3 == 0 ? 1 : 0) + (h % 7 == 0 ? 1 : 0); // 1 bis 3 Putts
        Spot spots[4];
        int count = 0;
        spots[count++] = Spot{0.0, 0.0}; // Abschlag
        if (hole.par >= 4) spots[count++] = Spot{length * 0.58, h % 2 ? 14.0 : -9.0};
        if (hole.par >= 5) spots[count++] = Spot{length * 0.85, 6.0};
        if (h % 4 == 1) spots[count++] = Spot{length - 28.0, -4.0}; // zusaetzlicher Chip
        playHole(s, h, spots, count, putts);
        expected_strokes += count + putts;
        expected_putts += putts;

        char msg[48];
        std::snprintf(msg, sizeof(msg), "Loch %d", h + 1);
        // Auch nach dem langen Weg zum naechsten Abschlag ist das Display nie
        // ganz aus; sonst wuerde ein Schlag nur wecken statt zaehlen.
        TEST_ASSERT_TRUE_MESSAGE(s.power().state() != ScreenState::Off, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(count + putts, s.app().round().shotCount(h), msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(putts, s.app().round().putts(h), msg);
        TEST_ASSERT_TRUE_MESSAGE(s.app().round().isFinished(h), msg);
        // Der Abschlag misst die geplante Weite, trotz Streuung.
        const ShotDistance tee_shot = shotDistance(s.app().round(), s.app().course(), h, 0, nullptr);
        const double planned = hole.par >= 4 ? std::sqrt(std::pow(length * 0.58, 2) + std::pow(h % 2 ? 14.0 : -9.0, 2))
                                             : (h % 4 == 1 ? std::sqrt(std::pow(length - 28.0, 2) + 16.0) : length - 6.0);
        TEST_ASSERT_TRUE_MESSAGE(tee_shot.available, msg);
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(8.0, planned, tee_shot.meters, msg);
    }

    ASSERT_OVERLAY(Overlay::RoundSummary, s.app());
    const RoundTotals totals = roundTotals(s.app().round(), s.app().course());
    TEST_ASSERT_EQUAL_INT(18, totals.holes_played);
    TEST_ASSERT_EQUAL_INT(expected_strokes, totals.strokes);
    TEST_ASSERT_EQUAL_INT(expected_putts, totals.putts);
    TEST_ASSERT_EQUAL_INT(expected_strokes - 72, totals.to_par);
    TEST_ASSERT_EQUAL_INT(0, s.failedSyncs());
    TEST_ASSERT_EQUAL_UINT32(0, s.feed().rejectedJumps()); // Streuung wird nie als Sprung verworfen
    TEST_ASSERT_TRUE(s.nowMs() > 2u * 3600u * 1000u);       // die Runde dauerte ueber zwei Stunden

    // Startzeit kam vom GPS, und die Runde uebersteht Neustart und Archiv.
    TEST_ASSERT_TRUE(s.session().roundStarted().valid);
    TEST_ASSERT_EQUAL_INT(8, s.session().roundStarted().hour);
    s.reboot();
    TEST_ASSERT_EQUAL_INT(expected_strokes, roundTotals(s.app().round(), s.app().course()).strokes);
    s.hold2();
    s.press2();
    s.press2();
    s.press1(); // Runde beenden
    s.hold1();  // neue Runde
    std::unique_ptr<RoundState> archived(new RoundState());
    TEST_ASSERT_TRUE(s.session().store().loadArchivedRound(1, *archived));
    TEST_ASSERT_EQUAL_INT(expected_strokes, roundTotals(archived->round, s.app().course()).strokes);
    TEST_ASSERT_EQUAL_INT(2026, archived->started.year);
}

// ---------- Stoerungen ----------

static void test_cold_start_without_fix() {
    // Am ersten Abschlag hat der Empfaenger noch keine Position und keine Uhrzeit.
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Hole hole = s.app().course().holes[0];
    s.placeAt(hole.tee);
    s.has_fix = false;
    s.has_time = false;
    s.wait(20.0);
    TEST_ASSERT_FALSE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_STRING("Kein GPS-Empfang", distanceScreenText(s.app()).hint);

    s.press1(); // trotzdem abschlagen
    s.press1();
    TEST_ASSERT_EQUAL_INT(1, s.app().round().shotCount(0));
    TEST_ASSERT_FALSE(s.app().round().shot(0, 0)->has_position);
    TEST_ASSERT_FALSE(s.session().roundStarted().valid);

    s.has_time = true; // unterwegs kommt zuerst die Zeit, dann die Position
    s.walkTo(positionOnHole(hole, 80.0));
    TEST_ASSERT_TRUE(s.session().roundStarted().valid); // Startzeit wird nachgetragen
    s.has_fix = true;
    s.walkTo(positionOnHole(hole, 200.0));
    s.wait(2.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(142, s.app().distances().center);
    TEST_ASSERT_EQUAL_STRING("1 Schlag", distanceScreenText(s.app()).foot); // Weite unbekannt, Zaehler stimmt
    s.press1();
    s.press1();
    TEST_ASSERT_TRUE(s.app().round().shot(0, 1)->has_position);
}

static void test_gps_dropout_in_the_middle_of_a_hole() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Hole hole = s.app().course().holes[1]; // Par 5
    s.placeAt(s.app().course().holes[0].tee);
    s.wait(2.0);
    s.press1();
    s.press1();
    s.press2();
    s.press2(); // Loch 2
    s.walkTo(hole.tee, 5.0);
    s.press1();
    s.press1();

    s.walkTo(positionOnHole(hole, 150.0));
    s.has_fix = false; // unter Baeumen
    s.walkTo(positionOnHole(hole, 230.0));
    TEST_ASSERT_FALSE(s.app().hasPosition());
    s.press1(); // Schlag ohne Position
    s.press1();
    s.has_fix = true;
    s.walkTo(positionOnHole(hole, 400.0));
    s.wait(2.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(68, s.app().distances().center);
    s.press1();
    s.press1();

    TEST_ASSERT_EQUAL_INT(3, s.app().round().shotCount(1));
    // Weiten, die den Schlag ohne Position beruehren, fehlen; erfunden wird nichts.
    TEST_ASSERT_FALSE(shotDistance(s.app().round(), s.app().course(), 1, 0, nullptr).available);
    TEST_ASSERT_FALSE(shotDistance(s.app().round(), s.app().course(), 1, 1, nullptr).available);
    const ShotsScreenText shots = shotsScreenText(s.app());
    TEST_ASSERT_EQUAL_STRING("", shots.rows[0].distance);
    TEST_ASSERT_EQUAL_STRING("", shots.rows[1].distance);
}

static void test_receiver_goes_silent_and_comes_back() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Hole hole = s.app().course().holes[0];
    s.placeAt(hole.tee);
    s.wait(3.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());

    s.receiver_on = false; // Empfaenger haengt, sendet nichts mehr
    s.wait(4.0);
    TEST_ASSERT_TRUE(s.app().hasPosition()); // kurze Luecke wird ueberbrueckt
    s.wait(4.0);
    TEST_ASSERT_FALSE(s.app().hasPosition()); // danach ehrlich "kein GPS"
    TEST_ASSERT_EQUAL_STRING("---", distanceScreenText(s.app()).center);

    s.walkTo(positionOnHole(hole, 100.0));
    s.receiver_on = true;
    s.wait(2.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(242, s.app().distances().center);
    TEST_ASSERT_EQUAL_UINT32(0, s.feed().rejectedJumps()); // nach der Pause kein falscher "Sprung"
}

static void test_power_cycle_in_the_middle_of_a_round() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Spot hole1[] = {{0.0, 0.0}, {200.0, 5.0}};
    playHole(s, 0, hole1, 2, 2);
    const Hole hole = s.app().course().holes[1];
    s.walkTo(hole.tee, 5.0);
    shotHere(s);
    s.walkTo(positionOnHole(hole, 220.0));

    s.reboot(); // Akku leer, spaeter wieder eingeschaltet
    TEST_ASSERT_FALSE(s.app().hasPosition()); // direkt nach dem Start noch keine Position
    s.wait(3.0);
    TEST_ASSERT_TRUE(s.app().hasPosition());
    TEST_ASSERT_EQUAL_INT(1, s.app().currentHole());
    TEST_ASSERT_EQUAL_INT(4, s.app().round().shotCount(0));
    TEST_ASSERT_EQUAL_INT(1, s.app().round().shotCount(1));
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 220 m", distanceScreenText(s.app()).foot);

    shotHere(s);
    TEST_ASSERT_EQUAL_INT(2, s.app().round().shotCount(1));
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 220.0, shotDistance(s.app().round(), s.app().course(), 1, 0, nullptr).meters);
}

static void test_noisy_serial_line_never_shows_a_wrong_place() {
    // Ein Prozent aller Zeichen kommt verfaelscht an (schlechter Kontakt).
    // Die angezeigte Position darf sich trotzdem nie weit von der echten entfernen.
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs, 77);
    s.corrupt_rate = 0.01;
    const Hole hole = s.app().course().holes[0]; // das aktuelle Loch, 342 m
    s.placeAt(hole.tee);

    double worst_error = 0.0;
    int checks = 0;
    for (int leg = 0; leg < 40; leg++) {
        // Zwanzig Mal die Bahn hinauf und hinunter: gut zweieinhalb Stunden Datenstrom.
        s.walkTo(leg % 2 ? hole.tee : hole.green_center, 1.4);
        if (s.app().hasPosition()) {
            const GreenDistances shown = s.app().distances();
            const double truth = distanceMeters(s.truePosition(), hole.green_center);
            const double error = std::fabs(shown.center - truth);
            if (error > worst_error) worst_error = error;
            checks++;
        }
    }
    TEST_ASSERT_TRUE(s.corrupted_bytes > 15000); // es wurde wirklich viel verfaelscht
    TEST_ASSERT_TRUE(checks > 30);
    TEST_ASSERT_TRUE_MESSAGE(worst_error < 12.0, "angezeigte Distanz wich stark von der echten ab");
}

// Geht die erste Bahn sechsmal hinauf und hinunter (rund 50 Minuten) und prueft
// jede Sekunde, ob die angezeigte Distanz zur echten passt. Erlaubt sind 12 m:
// Bei gestoerter Leitung darf die Position bis zu sechs Sekunden alt sein.
static int badSecondsUnderLineNoise(uint32_t seed, double corrupt_rate, int& seconds) {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs, seed);
    s.corrupt_rate = corrupt_rate;
    const Hole hole = s.app().course().holes[0];
    s.placeAt(hole.tee);
    s.wait(2.0);

    int bad_seconds = 0;
    seconds = 0;
    for (int leg = 0; leg < 12; leg++) {
        const LatLon target = leg % 2 ? hole.tee : hole.green_center;
        while (distanceMeters(s.truePosition(), target) > 2.0) {
            const double remaining = distanceMeters(s.truePosition(), target);
            const double f = 1.4 / remaining;
            LatLon next = s.truePosition();
            next.lat += (target.lat - next.lat) * f;
            next.lon += (target.lon - next.lon) * f;
            s.walkTo(next); // eine Sekunde gehen
            seconds++;
            if (!s.app().hasPosition()) continue;
            const double truth = distanceMeters(s.truePosition(), hole.green_center);
            if (std::fabs(s.app().distances().center - truth) > 12.0) bad_seconds++;
        }
    }
    return bad_seconds;
}

static const uint32_t kNoiseSeeds[] = {4711, 5, 99, 123456, 31337, 2026, 8, 1000003};

static void test_disturbed_serial_line_never_shows_a_wrong_distance() {
    // Jedes tausendste Zeichen falsch: weit schlechter als jede intakte Leitung.
    for (uint32_t seed : kNoiseSeeds) {
        int seconds = 0;
        const int bad = badSecondsUnderLineNoise(seed, 0.001, seconds);
        char msg[48];
        std::snprintf(msg, sizeof(msg), "Zufallsfolge %lu", static_cast<unsigned long>(seed));
        TEST_ASSERT_TRUE_MESSAGE(seconds > 2500, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, bad, msg);
    }
}

static void test_badly_broken_serial_line_is_wrong_only_for_moments() {
    // Jedes hundertste Zeichen falsch (defekter Kontakt): Etwa die Haelfte aller
    // Saetze ist unbrauchbar, und die schwache NMEA-Pruefsumme laesst ab und zu
    // einen verfaelschten durch. Ganz ausschliessen laesst sich eine falsche
    // Anzeige dann nicht, aber sie bleibt auf wenige Sekunden pro Stunde begrenzt.
    int total_bad = 0, total_seconds = 0;
    for (uint32_t seed : kNoiseSeeds) {
        int seconds = 0;
        const int bad = badSecondsUnderLineNoise(seed, 0.01, seconds);
        char msg[48];
        std::snprintf(msg, sizeof(msg), "Zufallsfolge %lu", static_cast<unsigned long>(seed));
        TEST_ASSERT_TRUE_MESSAGE(bad <= 6, msg);
        total_bad += bad;
        total_seconds += seconds;
    }
    TEST_ASSERT_TRUE(total_seconds > 20000);
    TEST_ASSERT_TRUE(total_bad * 1000 < total_seconds); // unter 0,1 % der Zeit
    char report[64];
    std::snprintf(report, sizeof(report), "%d von %d Sekunden falsch", total_bad, total_seconds);
    TEST_MESSAGE(report);
}

static void test_storage_failure_during_play_is_survived() {
    MemoryFs fs;
    installDemoCourse(fs);
    Scenario s(fs);
    const Hole hole = s.app().course().holes[0];
    s.placeAt(hole.tee);
    s.wait(2.0);
    shotHere(s);
    fs.read_only = true; // Karte herausgerutscht
    s.walkTo(positionOnHole(hole, 200.0));
    shotHere(s);
    TEST_ASSERT_TRUE(s.failedSyncs() > 0);
    TEST_ASSERT_EQUAL_INT(2, s.app().round().shotCount(0)); // gespielt wird weiter
    fs.read_only = false; // wieder drin
    s.wait(1.0);
    TEST_ASSERT_TRUE(s.session().lastSyncOk());
    s.reboot();
    TEST_ASSERT_EQUAL_INT(2, s.app().round().shotCount(0)); // nichts verloren
}

static void test_marking_greens_on_an_unknown_course() {
    // Platz ohne Daten: Gruens beim ersten Spielen selbst markieren.
    MemoryFs fs;
    Scenario s(fs);
    const Course real = demoCourse();
    s.placeAt(real.holes[0].tee);
    s.wait(2.0);
    TEST_ASSERT_EQUAL_STRING("Grün nicht markiert", distanceScreenText(s.app()).hint);
    shotHere(s);
    s.walkTo(real.holes[0].green_center, 5.0);
    s.hold2();
    s.press1(); // "Gruen hier markieren"
    TEST_ASSERT_TRUE(s.app().takeMessage() == Message::GreenMarked);
    shotHere(s); // Putt: jetzt ist das Gruen bekannt
    TEST_ASSERT_TRUE(s.app().round().shot(0, 1)->club == Club::Putter);

    s.reboot();
    s.placeAt(real.holes[0].tee);
    s.wait(3.0);
    TEST_ASSERT_EQUAL_INT(342, s.app().distances().center); // bleibt fuer die naechste Runde
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_one_hole_from_bytes_and_button_levels);
    RUN_TEST(test_long_press_undoes_and_opens_menu);
    RUN_TEST(test_full_round_with_realistic_gps_noise);
    RUN_TEST(test_cold_start_without_fix);
    RUN_TEST(test_gps_dropout_in_the_middle_of_a_hole);
    RUN_TEST(test_receiver_goes_silent_and_comes_back);
    RUN_TEST(test_power_cycle_in_the_middle_of_a_round);
    RUN_TEST(test_noisy_serial_line_never_shows_a_wrong_place);
    RUN_TEST(test_disturbed_serial_line_never_shows_a_wrong_distance);
    RUN_TEST(test_badly_broken_serial_line_is_wrong_only_for_moments);
    RUN_TEST(test_storage_failure_during_play_is_survived);
    RUN_TEST(test_marking_greens_on_an_unknown_course);
    return UNITY_END();
}
