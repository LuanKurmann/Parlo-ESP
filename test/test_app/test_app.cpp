#include <cstdio>
#include <unity.h>

#include "../test_support.h"
#include "core/app.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

#define ASSERT_OVERLAY(expected, app) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((app).overlay()))
#define ASSERT_VIEW(expected, app) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((app).view()))
#define ASSERT_CLUB(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))
#define ASSERT_MESSAGE(expected, app) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((app).takeMessage()))

// Alle Loecher liegen der Einfachheit halber am selben Ort: Abschlag bei kTee,
// Gruen `length` Meter noerdlich. Loch 1 Par 4 / 350 m, Loch 2 Par 3 / 150 m,
// Loch 3 Par 5 ohne Gruen-Daten, der Rest Par 4 / 300 m.
static Course makeCourse(int holes = 18) {
    Course c;
    c.hole_count = holes;
    for (int h = 0; h < kMaxHoles; h++) {
        c.holes[h].par = 4;
        c.holes[h].has_green = true;
        c.holes[h].green_center = offset(kTee, 300.0);
    }
    c.holes[0].green_center = offset(kTee, 350.0);
    c.holes[1].par = 3;
    c.holes[1].green_center = offset(kTee, 150.0);
    c.holes[2].par = 5;
    c.holes[2].has_green = false;
    return c;
}

static void walkTo(App& app, double metres_north) { app.setPosition(offset(kTee, metres_north)); }

// Schlag an der Position zaehlen und den vorgeschlagenen Schlaeger bestaetigen.
static void hit(App& app, double metres_north) {
    walkTo(app, metres_north);
    app.button1Short();
    app.button1Short();
}

// ---------- Start ----------

static void test_starts_on_hole_one_distance_view() {
    App app(makeCourse());
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
    ASSERT_VIEW(View::Distance, app);
    ASSERT_OVERLAY(Overlay::None, app);
    ASSERT_MESSAGE(Message::None, app);
    TEST_ASSERT_FALSE(app.hasPosition());
    TEST_ASSERT_EQUAL_INT(18, app.round().holeCount());
}

static void test_nine_hole_course_gives_nine_hole_round() {
    App app(makeCourse(9));
    TEST_ASSERT_EQUAL_INT(9, app.round().holeCount());
}

static void test_broken_hole_count_is_repaired() {
    App a(makeCourse(0));
    TEST_ASSERT_EQUAL_INT(1, a.round().holeCount());
    TEST_ASSERT_EQUAL_INT(1, a.course().hole_count);
    App b(makeCourse(500));
    TEST_ASSERT_EQUAL_INT(18, b.round().holeCount());
}

// ---------- Distanzen ----------

static void test_no_distances_without_gps() {
    App app(makeCourse());
    TEST_ASSERT_FALSE(app.distances().available);
}

static void test_distances_follow_position() {
    App app(makeCourse());
    walkTo(app, 0.0);
    TEST_ASSERT_EQUAL_INT(350, app.distances().center);
    walkTo(app, 205.0);
    TEST_ASSERT_EQUAL_INT(145, app.distances().center);
    TEST_ASSERT_EQUAL_INT(132, app.distances().front);
    TEST_ASSERT_EQUAL_INT(158, app.distances().back);
    app.clearPosition();
    TEST_ASSERT_FALSE(app.distances().available);
}

// ---------- Schlag und Schlaegerwahl ----------

static void test_button1_counts_shot_and_opens_club_picker() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short();
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    TEST_ASSERT_TRUE(app.round().shot(0, 0)->has_position);
    ASSERT_CLUB(Club::None, app.round().shot(0, 0)->club);
    ASSERT_CLUB(Club::Driver, app.suggestedClub());
}

static void test_button1_confirms_suggested_club() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short();
    app.button1Short();
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0)); // kein zweiter Schlag
    ASSERT_CLUB(Club::Driver, app.round().shot(0, 0)->club);
}

static void test_button2_cycles_clubs_in_picker() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short();
    app.button2Short();
    ASSERT_CLUB(Club::Wood3, app.suggestedClub());
    app.button2Short();
    ASSERT_CLUB(Club::Hybrid, app.suggestedClub());
    app.button1Short();
    ASSERT_CLUB(Club::Hybrid, app.round().shot(0, 0)->club);
    TEST_ASSERT_FALSE(app.round().isFinished(0)); // Knopf 2 hat das Loch nicht abgeschlossen
}

static void test_club_cycle_wraps_from_putter_to_driver() {
    App app(makeCourse());
    walkTo(app, 345.0); // auf dem Gruen: Putter vorgeschlagen
    app.button1Short();
    ASSERT_CLUB(Club::Putter, app.suggestedClub());
    app.button2Short();
    ASSERT_CLUB(Club::Driver, app.suggestedClub());
}

static void test_touch_selects_any_club() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short();
    app.selectClub(Club::Iron5);
    ASSERT_OVERLAY(Overlay::None, app);
    ASSERT_CLUB(Club::Iron5, app.round().shot(0, 0)->club);
}

static void test_touch_club_is_ignored_outside_picker_and_for_none() {
    App app(makeCourse());
    hit(app, 0.0);
    app.selectClub(Club::Putter); // Picker ist zu
    ASSERT_CLUB(Club::Driver, app.round().shot(0, 0)->club);
    app.button1Short();
    app.selectClub(Club::None); // kein gueltiger Schlaeger
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
}

static void test_suggestion_uses_remaining_distance() {
    App app(makeCourse());
    hit(app, 0.0);
    walkTo(app, 210.0); // 140 m bis zur Mitte
    app.button1Short();
    ASSERT_CLUB(Club::Iron7, app.suggestedClub());
    app.button1Short();
    walkTo(app, 348.0);
    app.button1Short();
    ASSERT_CLUB(Club::Putter, app.suggestedClub());
}

static void test_par3_tee_shot_suggestion() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Short(); // Loch 1 abschliessen
    app.button2Short(); // weiter
    TEST_ASSERT_EQUAL_INT(1, app.currentHole());
    walkTo(app, 0.0);
    app.button1Short();
    ASSERT_CLUB(Club::Iron6, app.suggestedClub()); // 150 m
}

static void test_shot_without_gps_is_counted_without_position() {
    App app(makeCourse());
    app.button1Short();
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    TEST_ASSERT_FALSE(app.round().shot(0, 0)->has_position);
    ASSERT_CLUB(Club::Driver, app.suggestedClub()); // Abschlag Par 4, Distanz unbekannt
    app.button1Short();
    app.button1Short();
    ASSERT_CLUB(Club::Iron7, app.suggestedClub()); // zweiter Schlag ohne Distanz
}

static void test_hole_without_green_still_works() {
    App app(makeCourse());
    for (int i = 0; i < 2; i++) { // zu Loch 3 (ohne Gruen-Daten)
        hit(app, 0.0);
        app.button2Short();
        app.button2Short();
    }
    TEST_ASSERT_EQUAL_INT(2, app.currentHole());
    walkTo(app, 0.0);
    TEST_ASSERT_FALSE(app.distances().available);
    app.button1Short();
    ASSERT_CLUB(Club::Driver, app.suggestedClub());
}

static void test_shot_limit_shows_message() {
    App app(makeCourse());
    for (int i = 0; i < kMaxShotsPerHole; i++) hit(app, 0.0);
    app.button1Short();
    ASSERT_MESSAGE(Message::HoleFull, app);
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(kMaxShotsPerHole, app.round().shotCount(0));
}

// ---------- Rueckgaengig ----------

static void test_long_button1_removes_last_shot() {
    App app(makeCourse());
    hit(app, 0.0);
    hit(app, 200.0);
    app.button1Long();
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    ASSERT_MESSAGE(Message::ShotRemoved, app);
    ASSERT_MESSAGE(Message::None, app); // Meldung kommt nur einmal
}

static void test_long_button1_in_picker_cancels_the_shot() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short();
    app.button1Long();
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
    ASSERT_MESSAGE(Message::ShotRemoved, app);
}

static void test_undo_without_shots_shows_message() {
    App app(makeCourse());
    app.button1Long();
    ASSERT_MESSAGE(Message::NoShotToRemove, app);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
}

// ---------- Schlagweite ----------

static void test_last_shot_distance_is_live() {
    App app(makeCourse());
    TEST_ASSERT_FALSE(app.lastShotDistance().available);
    hit(app, 0.0);
    walkTo(app, 100.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 100.0, app.lastShotDistance().meters);
    walkTo(app, 212.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 212.0, app.lastShotDistance().meters);
    app.clearPosition();
    TEST_ASSERT_FALSE(app.lastShotDistance().available);
}

// ---------- Loch abschliessen ----------

static void test_cannot_finish_hole_without_shot() {
    App app(makeCourse());
    app.button2Short();
    ASSERT_OVERLAY(Overlay::None, app);
    ASSERT_MESSAGE(Message::NoShotYet, app);
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
}

static void test_finish_hole_shows_result_then_moves_on() {
    App app(makeCourse());
    hit(app, 0.0);
    hit(app, 210.0);
    hit(app, 350.0);
    hit(app, 350.0);
    app.swipe(1); // Ansicht "Schlaege" offen lassen
    app.button2Short();
    ASSERT_OVERLAY(Overlay::HoleDone, app);
    TEST_ASSERT_TRUE(app.round().isFinished(0));
    TEST_ASSERT_EQUAL_INT(0, app.currentHole()); // Ergebnis gehoert noch zu Loch 1
    TEST_ASSERT_EQUAL_INT(2, app.round().putts(0));
    app.button2Short();
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(1, app.currentHole());
    ASSERT_VIEW(View::Distance, app); // neues Loch beginnt mit der Distanz
}

static void test_hole_done_continues_with_button1_or_tap() {
    App a(makeCourse());
    hit(a, 0.0);
    a.button2Short();
    a.button1Short();
    TEST_ASSERT_EQUAL_INT(1, a.currentHole());
    TEST_ASSERT_EQUAL_INT(0, a.round().shotCount(1)); // Knopf 1 hat keinen Schlag gezaehlt

    App b(makeCourse());
    hit(b, 0.0);
    b.button2Short();
    b.tap();
    TEST_ASSERT_EQUAL_INT(1, b.currentHole());
}

static void test_tap_does_nothing_elsewhere() {
    App app(makeCourse());
    app.tap();
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
    hit(app, 0.0);
    app.tap();
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
}

static void test_long_presses_do_nothing_on_hole_done() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Short();
    app.button1Long();
    app.button2Long();
    ASSERT_OVERLAY(Overlay::HoleDone, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

// ---------- Ansichten ----------

static void test_swipe_cycles_views_both_ways() {
    App app(makeCourse());
    app.swipe(1);
    ASSERT_VIEW(View::Shots, app);
    app.swipe(1);
    ASSERT_VIEW(View::Scorecard, app);
    app.swipe(1);
    ASSERT_VIEW(View::Distance, app);
    app.swipe(-1);
    ASSERT_VIEW(View::Scorecard, app);
    app.swipe(0);
    ASSERT_VIEW(View::Scorecard, app);
    app.swipe(-7);
    ASSERT_VIEW(View::Shots, app);
}

static void test_swipe_is_ignored_under_overlays() {
    App app(makeCourse());
    walkTo(app, 0.0);
    app.button1Short(); // Schlaegerwahl
    app.swipe(1);
    ASSERT_VIEW(View::Distance, app);
    app.button1Short();
    app.button2Long(); // Menue
    app.swipe(1);
    ASSERT_VIEW(View::Distance, app);
}

static void test_buttons_work_in_every_view() {
    App app(makeCourse());
    app.swipe(1);
    app.swipe(1); // Scorecard
    walkTo(app, 0.0);
    app.button1Short();
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
    app.button1Short();
    ASSERT_VIEW(View::Scorecard, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

// ---------- Menue ----------

static void test_long_button2_opens_and_closes_menu() {
    App app(makeCourse());
    app.button2Long();
    ASSERT_OVERLAY(Overlay::Menu, app);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::MarkGreen), static_cast<int>(app.menuSelection()));
    app.button2Long();
    ASSERT_OVERLAY(Overlay::None, app);
}

static void test_menu_cannot_open_during_club_picker() {
    App app(makeCourse());
    app.button1Short();
    app.button2Long();
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
}

static void test_menu_selection_cycles_and_resets_on_reopen() {
    App app(makeCourse());
    app.button2Long();
    for (int i = 0; i < kMenuItemCount; i++) app.button2Short();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::MarkGreen), static_cast<int>(app.menuSelection()));
    app.button2Short();
    app.button2Short();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::EndRound), static_cast<int>(app.menuSelection()));
    app.button2Long();
    app.button2Long();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::MarkGreen), static_cast<int>(app.menuSelection()));
}

static void test_menu_mark_green_with_buttons() {
    App app(makeCourse());
    for (int i = 0; i < 2; i++) { // zu Loch 3 ohne Gruen
        hit(app, 0.0);
        app.button2Short();
        app.button2Short();
    }
    walkTo(app, 470.0);
    app.button2Long();
    app.button1Short(); // "Gruen hier markieren"
    ASSERT_OVERLAY(Overlay::None, app);
    ASSERT_MESSAGE(Message::GreenMarked, app);
    TEST_ASSERT_TRUE(app.course().holes[2].has_green);
    walkTo(app, 300.0);
    TEST_ASSERT_EQUAL_INT(170, app.distances().center);
    TEST_ASSERT_TRUE(app.course().holes[3].has_green);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(2)); // Knopf 1 hat keinen Schlag gezaehlt
}

static void test_mark_green_without_gps_changes_nothing() {
    App app(makeCourse());
    app.button2Long();
    app.selectMenuItem(MenuItem::MarkGreen);
    ASSERT_MESSAGE(Message::NoGpsForGreen, app);
    walkTo(app, 0.0);
    TEST_ASSERT_EQUAL_INT(350, app.distances().center); // importiertes Gruen unveraendert
}

static void test_menu_previous_hole() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Short();
    app.button2Short();
    app.button2Long();
    app.button2Short();
    app.button1Short(); // "Vorheriges Loch"
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
    TEST_ASSERT_TRUE(app.round().isFinished(0)); // Daten bleiben erhalten
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    ASSERT_MESSAGE(Message::None, app);
}

static void test_previous_hole_on_first_hole_shows_message() {
    App app(makeCourse());
    app.button2Long();
    app.selectMenuItem(MenuItem::PreviousHole);
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
    ASSERT_MESSAGE(Message::AlreadyFirstHole, app);
}

static void test_correcting_a_previous_hole() {
    // Zurueck, einen vergessenen Putt nachtragen, wieder abschliessen.
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Short();
    app.button2Short();
    app.button2Long();
    app.selectMenuItem(MenuItem::PreviousHole);
    hit(app, 350.0);
    TEST_ASSERT_FALSE(app.round().isFinished(0));
    app.button2Short();
    app.button2Short();
    TEST_ASSERT_EQUAL_INT(1, app.currentHole());
    TEST_ASSERT_EQUAL_INT(2, app.round().shotCount(0));
    TEST_ASSERT_EQUAL_INT(1, app.round().putts(0));
}

static void test_menu_close_and_touch_outside_menu() {
    App app(makeCourse());
    app.button2Long();
    app.selectMenuItem(MenuItem::Close);
    ASSERT_OVERLAY(Overlay::None, app);
    app.selectMenuItem(MenuItem::EndRound); // Menue ist zu
    ASSERT_OVERLAY(Overlay::None, app);
}

static void test_long_button1_does_nothing_in_menu() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Long();
    app.button1Long();
    ASSERT_OVERLAY(Overlay::Menu, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

// ---------- Rundenende ----------

static void test_end_round_from_menu_and_back() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Long();
    app.selectMenuItem(MenuItem::EndRound);
    ASSERT_OVERLAY(Overlay::RoundSummary, app);
    app.button2Short(); // zurueck
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

static void test_short_button1_never_discards_the_round() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Long();
    app.selectMenuItem(MenuItem::EndRound);
    app.button1Short();
    ASSERT_OVERLAY(Overlay::RoundSummary, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

static void test_long_button1_starts_new_round() {
    App app(makeCourse());
    hit(app, 0.0);
    app.button2Short();
    app.button2Short();
    hit(app, 0.0);
    app.swipe(1);
    app.button2Long();
    app.selectMenuItem(MenuItem::EndRound);
    app.button1Long();
    ASSERT_OVERLAY(Overlay::None, app);
    ASSERT_MESSAGE(Message::NewRoundStarted, app);
    TEST_ASSERT_EQUAL_INT(0, app.currentHole());
    ASSERT_VIEW(View::Distance, app);
    for (int h = 0; h < 18; h++) {
        TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(h));
        TEST_ASSERT_FALSE(app.round().isFinished(h));
    }
}

static void test_marked_green_survives_new_round() {
    App app(makeCourse());
    walkTo(app, 400.0);
    app.button2Long();
    app.selectMenuItem(MenuItem::MarkGreen);
    app.button2Long();
    app.selectMenuItem(MenuItem::EndRound);
    app.button1Long();
    walkTo(app, 0.0);
    TEST_ASSERT_EQUAL_INT(400, app.distances().center);
}

static void test_last_hole_leads_to_round_summary() {
    App app(makeCourse(9));
    for (int h = 0; h < 9; h++) {
        TEST_ASSERT_EQUAL_INT(h, app.currentHole());
        hit(app, 0.0);
        app.button2Short();
        app.button2Short();
    }
    ASSERT_OVERLAY(Overlay::RoundSummary, app);
    TEST_ASSERT_EQUAL_INT(8, app.currentHole());
    TEST_ASSERT_EQUAL_INT(9, roundTotals(app.round(), app.course()).holes_played);
}

// ---------- Ganze Runde nur mit den Knoepfen ----------

static void test_full_round_with_buttons_only() {
    App app(makeCourse());
    int expected_strokes = 0;
    for (int h = 0; h < 18; h++) {
        const bool has_green = app.course().holes[h].has_green;
        const double length = h == 0 ? 350.0 : h == 1 ? 150.0 : 300.0;
        // Abschlag, ein Schlag aus 100 m (ausser Par 3), zwei Putts.
        hit(app, 0.0);
        expected_strokes++;
        if (h != 1) {
            hit(app, length - 100.0);
            expected_strokes++;
        }
        for (int putt = 0; putt < 2; putt++) {
            walkTo(app, length);
            app.button1Short();
            if (has_green) {
                ASSERT_CLUB(Club::Putter, app.suggestedClub());
                app.button1Short();
            } else {
                app.selectClub(Club::Putter); // ohne Gruen-Daten kein Putter-Vorschlag
            }
            expected_strokes++;
        }
        app.button2Short();
        ASSERT_OVERLAY(Overlay::HoleDone, app);
        app.button2Short();
    }
    ASSERT_OVERLAY(Overlay::RoundSummary, app);
    const RoundTotals t = roundTotals(app.round(), app.course());
    TEST_ASSERT_EQUAL_INT(18, t.holes_played);
    TEST_ASSERT_EQUAL_INT(71, expected_strokes);
    TEST_ASSERT_EQUAL_INT(71, t.strokes);
    TEST_ASSERT_EQUAL_INT(36, t.putts);
    // Par 72; Loch 2 (Par 3) in 3, Loch 3 (Par 5) in 4, sonst Par.
    TEST_ASSERT_EQUAL_INT(-1, t.to_par);
    TEST_ASSERT_DOUBLE_WITHIN(0.05, 250.0, t.longest_shot_m);
}

// ---------- Knopf-Ereignisse weiterleiten ----------

static void test_dispatch_button_events() {
    App app(makeCourse());
    dispatchButton(app, 1, ButtonEvent::None);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
    dispatchButton(app, 1, ButtonEvent::ShortPress);
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
    dispatchButton(app, 1, ButtonEvent::LongPress);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
    dispatchButton(app, 2, ButtonEvent::LongPress);
    ASSERT_OVERLAY(Overlay::Menu, app);
    dispatchButton(app, 2, ButtonEvent::ShortPress);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::PreviousHole), static_cast<int>(app.menuSelection()));
    dispatchButton(app, 3, ButtonEvent::ShortPress); // unbekannter Knopf
    dispatchButton(app, 0, ButtonEvent::LongPress);
    ASSERT_OVERLAY(Overlay::Menu, app);
}

static void test_real_button_timing_drives_the_app() {
    // Knopf 1 kurz druecken, dann lang halten: Schlag zaehlen und wieder loeschen.
    App app(makeCourse());
    Button b1;
    uint32_t t = 0;
    auto hold = [&](bool pressed, uint32_t ms) {
        for (uint32_t end = t + ms; t < end; t += 5) dispatchButton(app, 1, b1.update(pressed, t));
    };
    hold(false, 100);
    hold(true, 150);
    hold(false, 100);
    ASSERT_OVERLAY(Overlay::ClubPicker, app);
    hold(true, 800);
    ASSERT_OVERLAY(Overlay::None, app);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
    hold(false, 100);
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0)); // Loslassen nach Lang zaehlt keinen Schlag
}

// ---------- Zufaellige Bedienung ----------

static void test_random_inputs_keep_state_consistent() {
    // 300'000 zufaellige Eingaben (fester Startwert). Nach jeder Eingabe muss
    // der Zustand in sich stimmig sein.
    App app(makeCourse());
    uint32_t state = 2026;
    for (int i = 0; i < 300000; i++) {
        state = state * 1664525u + 1013904223u;
        const uint32_t r = state >> 8;
        switch (r % 14) {
            case 0:
            case 1:
            case 2: app.button1Short(); break;
            case 3: app.button1Long(); break;
            case 4:
            case 5: app.button2Short(); break;
            case 6: app.button2Long(); break;
            case 7: app.swipe(static_cast<int>(r >> 4) % 3 - 1); break;
            case 8: app.selectClub(static_cast<Club>((r >> 4) % (kClubCount + 2))); break;
            case 9: app.selectMenuItem(static_cast<MenuItem>((r >> 4) % (kMenuItemCount + 2))); break;
            case 10: app.tap(); break;
            case 11: walkTo(app, static_cast<double>((r >> 4) % 500)); break;
            case 12: app.clearPosition(); break;
            case 13: app.takeMessage(); break;
        }

        const int hole = app.currentHole();
        TEST_ASSERT_TRUE(hole >= 0 && hole < 18);
        TEST_ASSERT_TRUE(static_cast<int>(app.view()) < kViewCount);
        TEST_ASSERT_TRUE(static_cast<int>(app.menuSelection()) < kMenuItemCount);
        TEST_ASSERT_TRUE(static_cast<int>(app.suggestedClub()) < kClubCount);

        const int count = app.round().shotCount(hole);
        if (app.overlay() == Overlay::ClubPicker) {
            // Die Schlaegerwahl gehoert immer zum letzten, noch offenen Schlag.
            TEST_ASSERT_TRUE(count > 0);
            TEST_ASSERT_TRUE(app.round().shot(hole, count - 1)->club == Club::None);
        }
        if (app.overlay() == Overlay::HoleDone) TEST_ASSERT_TRUE(app.round().isFinished(hole));

        // Ausserhalb der Schlaegerwahl hat jeder Schlag einen Schlaeger.
        if (i % 97 == 0 && app.overlay() != Overlay::ClubPicker) {
            for (int h = 0; h < 18; h++) {
                for (int s = 0; s < app.round().shotCount(h); s++) {
                    TEST_ASSERT_TRUE(app.round().shot(h, s)->club != Club::None);
                }
            }
        }
    }
}

// ---------- Mit Handy verbinden ----------

static void test_phone_screen_opens_from_menu_and_wants_wifi() {
    App app(makeCourse());
    TEST_ASSERT_FALSE(app.phoneWanted());
    app.button2Long();
    app.button2Short();
    app.button2Short();
    app.button2Short(); // vierter Eintrag
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MenuItem::Phone), static_cast<int>(app.menuSelection()));
    app.button1Short();
    ASSERT_OVERLAY(Overlay::Phone, app);
    TEST_ASSERT_TRUE(app.phoneWanted());
}

static void openPhone(App& app) {
    app.button2Long();
    app.selectMenuItem(MenuItem::Phone);
}

static void test_phone_screen_closes_on_any_short_press_or_tap() {
    for (int way = 0; way < 3; way++) {
        App app(makeCourse());
        hit(app, 0.0);
        openPhone(app);
        if (way == 0) app.button1Short();
        if (way == 1) app.button2Short();
        if (way == 2) app.tap();
        ASSERT_OVERLAY(Overlay::None, app);
        TEST_ASSERT_FALSE(app.phoneWanted());
        TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0)); // kein Schlag gezaehlt, kein Loch abgeschlossen
        TEST_ASSERT_FALSE(app.round().isFinished(0));
    }
}

static void test_phone_screen_ignores_long_presses_and_swipes() {
    App app(makeCourse());
    hit(app, 0.0);
    openPhone(app);
    app.button1Long();
    app.button2Long();
    app.swipe(1);
    app.selectClub(Club::Putter);
    ASSERT_OVERLAY(Overlay::Phone, app);
    ASSERT_VIEW(View::Distance, app);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

static void test_net_status_is_kept_for_display() {
    App app(makeCourse());
    TEST_ASSERT_TRUE(app.netStatus().mode == NetMode::Off);
    NetStatus status;
    status.mode = NetMode::Hotspot;
    std::snprintf(status.ssid, sizeof(status.ssid), "Golf-Tracker");
    app.setNetStatus(status);
    TEST_ASSERT_TRUE(app.netStatus().mode == NetMode::Hotspot);
    TEST_ASSERT_EQUAL_STRING("Golf-Tracker", app.netStatus().ssid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_on_hole_one_distance_view);
    RUN_TEST(test_nine_hole_course_gives_nine_hole_round);
    RUN_TEST(test_broken_hole_count_is_repaired);
    RUN_TEST(test_no_distances_without_gps);
    RUN_TEST(test_distances_follow_position);
    RUN_TEST(test_button1_counts_shot_and_opens_club_picker);
    RUN_TEST(test_button1_confirms_suggested_club);
    RUN_TEST(test_button2_cycles_clubs_in_picker);
    RUN_TEST(test_club_cycle_wraps_from_putter_to_driver);
    RUN_TEST(test_touch_selects_any_club);
    RUN_TEST(test_touch_club_is_ignored_outside_picker_and_for_none);
    RUN_TEST(test_suggestion_uses_remaining_distance);
    RUN_TEST(test_par3_tee_shot_suggestion);
    RUN_TEST(test_shot_without_gps_is_counted_without_position);
    RUN_TEST(test_hole_without_green_still_works);
    RUN_TEST(test_shot_limit_shows_message);
    RUN_TEST(test_long_button1_removes_last_shot);
    RUN_TEST(test_long_button1_in_picker_cancels_the_shot);
    RUN_TEST(test_undo_without_shots_shows_message);
    RUN_TEST(test_last_shot_distance_is_live);
    RUN_TEST(test_cannot_finish_hole_without_shot);
    RUN_TEST(test_finish_hole_shows_result_then_moves_on);
    RUN_TEST(test_hole_done_continues_with_button1_or_tap);
    RUN_TEST(test_tap_does_nothing_elsewhere);
    RUN_TEST(test_long_presses_do_nothing_on_hole_done);
    RUN_TEST(test_swipe_cycles_views_both_ways);
    RUN_TEST(test_swipe_is_ignored_under_overlays);
    RUN_TEST(test_buttons_work_in_every_view);
    RUN_TEST(test_long_button2_opens_and_closes_menu);
    RUN_TEST(test_menu_cannot_open_during_club_picker);
    RUN_TEST(test_menu_selection_cycles_and_resets_on_reopen);
    RUN_TEST(test_menu_mark_green_with_buttons);
    RUN_TEST(test_mark_green_without_gps_changes_nothing);
    RUN_TEST(test_menu_previous_hole);
    RUN_TEST(test_previous_hole_on_first_hole_shows_message);
    RUN_TEST(test_correcting_a_previous_hole);
    RUN_TEST(test_menu_close_and_touch_outside_menu);
    RUN_TEST(test_long_button1_does_nothing_in_menu);
    RUN_TEST(test_end_round_from_menu_and_back);
    RUN_TEST(test_short_button1_never_discards_the_round);
    RUN_TEST(test_long_button1_starts_new_round);
    RUN_TEST(test_marked_green_survives_new_round);
    RUN_TEST(test_last_hole_leads_to_round_summary);
    RUN_TEST(test_full_round_with_buttons_only);
    RUN_TEST(test_dispatch_button_events);
    RUN_TEST(test_real_button_timing_drives_the_app);
    RUN_TEST(test_random_inputs_keep_state_consistent);
    RUN_TEST(test_phone_screen_opens_from_menu_and_wants_wifi);
    RUN_TEST(test_phone_screen_closes_on_any_short_press_or_tap);
    RUN_TEST(test_phone_screen_ignores_long_presses_and_swipes);
    RUN_TEST(test_net_status_is_kept_for_display);
    return UNITY_END();
}
