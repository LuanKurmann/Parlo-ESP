#include <cstdio>
#include <cstring>

#include <unity.h>

#include "../test_support.h"
#include "core/display_text.h"

using namespace golf;
using testsupport::kTee;
using testsupport::offset;

void setUp() {}
void tearDown() {}

static Course makeCourse() {
    Course c;
    c.holes[0].par = 5;
    c.holes[0].has_tee = true;
    c.holes[0].tee = kTee;
    c.holes[0].has_green = true;
    c.holes[0].green_center = offset(kTee, 487.0);
    c.holes[1].par = 3; // Gruen und Abschlag unbekannt
    c.holes[2].par = 0;
    c.holes[2].has_green = true; // Gruen selbst markiert, kein Abschlag, kein Par
    c.holes[2].green_center = offset(kTee, 300.0);
    return c;
}

static void hit(App& app, double metres_north) {
    app.setPosition(offset(kTee, metres_north));
    app.button1Short();
    app.button1Short();
}

static void nextHole(App& app) {
    hit(app, 0.0);
    app.button2Short();
    app.button2Short();
}

static void test_tee_of_first_hole() {
    App app(makeCourse());
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 1", t.title);
    TEST_ASSERT_EQUAL_STRING("Par 5 · 487 m", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("500", t.back);
    TEST_ASSERT_EQUAL_STRING("487", t.center);
    TEST_ASSERT_EQUAL_STRING("474", t.front);
    TEST_ASSERT_EQUAL_STRING("Noch kein Schlag", t.foot);
    TEST_ASSERT_EQUAL_STRING("", t.hint);
    TEST_ASSERT_TRUE(t.gps_ok);
}

static void test_as_in_the_prototype_after_the_drive() {
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, 218.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("282", t.back);
    TEST_ASSERT_EQUAL_STRING("269", t.center);
    TEST_ASSERT_EQUAL_STRING("256", t.front);
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 218 m", t.foot);
}

static void test_plural_and_putt_without_distance() {
    App app(makeCourse());
    hit(app, 0.0);
    hit(app, 218.0);
    app.setPosition(offset(kTee, 380.0));
    TEST_ASSERT_EQUAL_STRING("2 Schläge · letzter 162 m", distanceScreenText(app).foot);
    hit(app, 485.0); // Putter vorgeschlagen und bestaetigt
    TEST_ASSERT_EQUAL_STRING("3 Schläge", distanceScreenText(app).foot);
}

static void test_no_gps() {
    App app(makeCourse());
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("---", t.center);
    TEST_ASSERT_EQUAL_STRING("---", t.front);
    TEST_ASSERT_EQUAL_STRING("---", t.back);
    TEST_ASSERT_EQUAL_STRING("Kein GPS-Empfang", t.hint);
    TEST_ASSERT_FALSE(t.gps_ok);
    TEST_ASSERT_EQUAL_STRING("Par 5 · 487 m", t.subtitle); // Lochdaten bleiben sichtbar
}

static void test_shot_counted_without_gps_shows_no_distance() {
    App app(makeCourse());
    app.button1Short();
    app.button1Short();
    TEST_ASSERT_EQUAL_STRING("1 Schlag", distanceScreenText(app).foot);
}

static void test_hole_without_green() {
    App app(makeCourse());
    nextHole(app);
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 2", t.title);
    TEST_ASSERT_EQUAL_STRING("Par 3", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("---", t.center);
    TEST_ASSERT_EQUAL_STRING("Grün nicht markiert", t.hint);
    TEST_ASSERT_TRUE(t.gps_ok);
}

static void test_no_gps_wins_over_missing_green() {
    App app(makeCourse());
    nextHole(app);
    app.clearPosition();
    TEST_ASSERT_EQUAL_STRING("Kein GPS-Empfang", distanceScreenText(app).hint);
}

static void test_hole_without_par_and_tee() {
    App app(makeCourse());
    nextHole(app);
    nextHole(app);
    app.setPosition(kTee);
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 3", t.title);
    TEST_ASSERT_EQUAL_STRING("", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("300", t.center);
}

static void test_length_without_par() {
    Course c = makeCourse();
    c.holes[0].par = 0;
    App app(c);
    TEST_ASSERT_EQUAL_STRING("487 m", distanceScreenText(app).subtitle);
}

static void test_hole_18_title_fits() {
    Course c = makeCourse();
    App app(c);
    for (int i = 0; i < 17; i++) nextHole(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 18", distanceScreenText(app).title);
}

static void test_far_away_is_capped_at_999() {
    // Auf dem Parkplatz oder zu Hause: nie mehr als drei Ziffern.
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, -25000.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("999", t.back);
    TEST_ASSERT_EQUAL_STRING("999", t.center);
    TEST_ASSERT_EQUAL_STRING("999", t.front);
    TEST_ASSERT_EQUAL_STRING("1 Schlag · letzter 999 m", t.foot);
}

static void test_on_the_green() {
    App app(makeCourse());
    app.setPosition(offset(kTee, 487.0));
    const DistanceScreenText t = distanceScreenText(app);
    TEST_ASSERT_EQUAL_STRING("0", t.center);
    TEST_ASSERT_EQUAL_STRING("13", t.front);
    TEST_ASSERT_EQUAL_STRING("13", t.back);
}

static void test_twenty_shots_fit() {
    App app(makeCourse());
    for (int i = 0; i < kMaxShotsPerHole; i++) hit(app, i * 10.0);
    app.setPosition(offset(kTee, 400.0));
    TEST_ASSERT_EQUAL_STRING("20 Schläge · letzter 210 m", distanceScreenText(app).foot);
}

// ---------- Ansicht "Schlaege" ----------

static void test_shots_view_empty() {
    App app(makeCourse());
    const ShotsScreenText t = shotsScreenText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 1", t.title);
    TEST_ASSERT_EQUAL_INT(0, t.count);
}

static void test_shots_view_lists_clubs_and_distances() {
    App app(makeCourse());
    hit(app, 0.0);   // Driver
    hit(app, 218.0); // 269 m bis zur Mitte: Driver ist am naechsten
    hit(app, 380.0); // 107 m: Pitching Wedge
    hit(app, 485.0); // Putter
    app.setPosition(offset(kTee, 487.0));
    const ShotsScreenText t = shotsScreenText(app);
    TEST_ASSERT_EQUAL_INT(4, t.count);
    TEST_ASSERT_EQUAL_STRING("1", t.rows[0].number);
    TEST_ASSERT_EQUAL_STRING("Driver", t.rows[0].club);
    TEST_ASSERT_EQUAL_STRING("218 m", t.rows[0].distance);
    TEST_ASSERT_EQUAL_STRING("162 m", t.rows[1].distance);
    TEST_ASSERT_EQUAL_STRING("Pitching Wedge", t.rows[2].club);
    TEST_ASSERT_EQUAL_STRING("105 m", t.rows[2].distance);
    TEST_ASSERT_EQUAL_STRING("4", t.rows[3].number);
    TEST_ASSERT_EQUAL_STRING("Putter", t.rows[3].club);
    TEST_ASSERT_EQUAL_STRING("Putt", t.rows[3].distance);
}

static void test_shots_view_last_shot_is_live_and_open_club_is_a_dash() {
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, 100.0));
    TEST_ASSERT_EQUAL_STRING("100 m", shotsScreenText(app).rows[0].distance);
    app.setPosition(offset(kTee, 150.0));
    TEST_ASSERT_EQUAL_STRING("150 m", shotsScreenText(app).rows[0].distance);
    app.button1Short(); // zweiter Schlag, Schlaegerwahl noch offen
    const ShotsScreenText t = shotsScreenText(app);
    TEST_ASSERT_EQUAL_STRING("–", t.rows[1].club);
    TEST_ASSERT_EQUAL_STRING("0 m", t.rows[1].distance);
}

static void test_shots_view_without_gps_has_empty_distances() {
    App app(makeCourse());
    app.button1Short();
    app.button1Short();
    app.button1Short();
    app.button1Short();
    const ShotsScreenText t = shotsScreenText(app);
    TEST_ASSERT_EQUAL_INT(2, t.count);
    TEST_ASSERT_EQUAL_STRING("", t.rows[0].distance);
    TEST_ASSERT_EQUAL_STRING("", t.rows[1].distance);
}

static void test_shots_view_with_twenty_shots() {
    App app(makeCourse());
    for (int i = 0; i < kMaxShotsPerHole; i++) hit(app, i * 10.0);
    const ShotsScreenText t = shotsScreenText(app);
    TEST_ASSERT_EQUAL_INT(20, t.count);
    TEST_ASSERT_EQUAL_STRING("20", t.rows[19].number);
    TEST_ASSERT_EQUAL_STRING("10 m", t.rows[0].distance);
}

// ---------- Ansicht "Scorecard" ----------

// Spielt das aktuelle Loch mit `strokes` Schlaegen, davon `putts` mit dem Putter.
static void playHole(App& app, int strokes, int putts) {
    for (int i = 0; i < strokes; i++) {
        app.clearPosition();
        app.button1Short();
        app.selectClub(i >= strokes - putts ? Club::Putter : Club::Iron7);
    }
    app.button2Short();
    app.button2Short();
}

static void test_scorecard_of_new_round() {
    App app(makeCourse());
    const ScorecardText t = scorecardText(app);
    TEST_ASSERT_EQUAL_STRING("Noch kein Loch gespielt", t.subtitle);
    TEST_ASSERT_EQUAL_INT(18, t.hole_count);
    TEST_ASSERT_EQUAL_STRING("1", t.cells[0].hole);
    TEST_ASSERT_EQUAL_STRING("18", t.cells[17].hole);
    TEST_ASSERT_EQUAL_STRING("–", t.cells[0].score);
    TEST_ASSERT_TRUE(t.cells[0].tone == ScoreTone::None);
    TEST_ASSERT_TRUE(t.cells[0].current);
    TEST_ASSERT_FALSE(t.cells[1].current);
    TEST_ASSERT_EQUAL_STRING("0 Schläge", t.strokes);
    TEST_ASSERT_EQUAL_STRING("–", t.to_par);
    TEST_ASSERT_EQUAL_STRING("0 Putts", t.putts);
}

static void test_scorecard_colours_and_totals() {
    Course c = makeCourse();
    c.holes[2].par = 4;
    c.holes[3].par = 4;
    App app(c);
    playHole(app, 4, 1); // Par 5: Birdie
    playHole(app, 3, 2); // Par 3: Par
    playHole(app, 6, 3); // Par 4: Doppelbogey
    app.button1Short();  // Loch 4 laeuft, 1 Schlag
    app.button1Short();
    const ScorecardText t = scorecardText(app);
    TEST_ASSERT_EQUAL_STRING("3 Löcher gespielt", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("4", t.cells[0].score);
    TEST_ASSERT_TRUE(t.cells[0].tone == ScoreTone::Under);
    TEST_ASSERT_TRUE(t.cells[1].tone == ScoreTone::Par);
    TEST_ASSERT_EQUAL_STRING("6", t.cells[2].score);
    TEST_ASSERT_TRUE(t.cells[2].tone == ScoreTone::Over);
    TEST_ASSERT_EQUAL_STRING("1", t.cells[3].score); // laufendes Loch zeigt den Zwischenstand
    TEST_ASSERT_TRUE(t.cells[3].tone == ScoreTone::None);
    TEST_ASSERT_TRUE(t.cells[3].current);
    TEST_ASSERT_FALSE(t.cells[0].current);
    TEST_ASSERT_EQUAL_STRING("13 Schläge", t.strokes); // nur abgeschlossene Loecher
    TEST_ASSERT_EQUAL_STRING("+1", t.to_par);
    TEST_ASSERT_TRUE(t.to_par_tone == ScoreTone::Over);
    TEST_ASSERT_EQUAL_STRING("6 Putts", t.putts);
}

static void test_scorecard_singular_and_even_par() {
    App app(makeCourse());
    playHole(app, 5, 1);
    const ScorecardText t = scorecardText(app);
    TEST_ASSERT_EQUAL_STRING("1 Loch gespielt", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("E", t.to_par);
    TEST_ASSERT_TRUE(t.to_par_tone == ScoreTone::Par);
    TEST_ASSERT_EQUAL_STRING("1 Putt", t.putts);
}

static void test_scorecard_under_par_and_hole_without_par() {
    App app(makeCourse());
    playHole(app, 3, 1); // Par 5 in 3: -2
    playHole(app, 1, 0); // Par 3 in 1
    playHole(app, 7, 2); // Par unbekannt
    const ScorecardText t = scorecardText(app);
    TEST_ASSERT_EQUAL_STRING("-4", t.to_par);
    TEST_ASSERT_TRUE(t.to_par_tone == ScoreTone::Under);
    TEST_ASSERT_TRUE(t.cells[2].tone == ScoreTone::Par); // ohne Par neutral
    TEST_ASSERT_EQUAL_STRING("11 Schläge", t.strokes);
}

static void test_scorecard_nine_holes_and_two_digit_scores() {
    Course c = makeCourse();
    c.hole_count = 9;
    App app(c);
    playHole(app, 20, 5);
    const ScorecardText t = scorecardText(app);
    TEST_ASSERT_EQUAL_INT(9, t.hole_count);
    TEST_ASSERT_EQUAL_STRING("20", t.cells[0].score);
    TEST_ASSERT_EQUAL_STRING("+15", t.to_par);
}

// ---------- Schlaegerwahl und Menue ----------

static void test_club_picker_text() {
    App app(makeCourse());
    hit(app, 0.0);
    app.setPosition(offset(kTee, 347.0));
    app.button1Short();
    ClubPickerText t = clubPickerText(app);
    TEST_ASSERT_EQUAL_STRING("Noch 140 m bis Grünmitte", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("Eisen 7 vorgeschlagen", t.hint);
    TEST_ASSERT_EQUAL_INT(11, t.count); // Standard-Bag
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Driver), t.clubs[0]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Putter), t.clubs[10]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Iron7), t.clubs[t.suggested]);
    app.button2Short();
    t = clubPickerText(app);
    TEST_ASSERT_EQUAL_STRING("Eisen 8 vorgeschlagen", t.hint);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Iron8), t.clubs[t.suggested]);
}

static void test_club_picker_shows_only_the_clubs_in_the_bag() {
    Bag bag;
    for (int i = 0; i < kClubCount; i++) bag.in_bag[i] = false;
    bag.in_bag[static_cast<int>(Club::Wood5)] = true;
    bag.in_bag[static_cast<int>(Club::Iron8)] = true;
    bag.in_bag[static_cast<int>(Club::LobWedge)] = true;
    bag.in_bag[static_cast<int>(Club::Putter)] = true;
    App app(makeCourse());
    app.setBag(bag);
    app.setPosition(kTee);
    app.button1Short();
    const ClubPickerText t = clubPickerText(app);
    TEST_ASSERT_EQUAL_INT(4, t.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Wood5), t.clubs[0]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Iron8), t.clubs[1]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::LobWedge), t.clubs[2]);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Club::Putter), t.clubs[3]);
    TEST_ASSERT_EQUAL_INT(0, t.suggested); // Abschlag Par 5: der laengste
    TEST_ASSERT_EQUAL_STRING("Holz 5 vorgeschlagen", t.hint);
}

static void test_club_picker_with_a_full_bag_of_14() {
    Bag bag;
    for (int i = 0; i < kClubCount; i++) bag.in_bag[i] = i < 13 || static_cast<Club>(i) == Club::Putter;
    App app(makeCourse());
    app.setBag(bag);
    app.button1Short();
    const ClubPickerText t = clubPickerText(app);
    TEST_ASSERT_EQUAL_INT(14, t.count);
    TEST_ASSERT_TRUE(t.suggested >= 0 && t.suggested < 14);
}

static void test_club_picker_without_distance() {
    App app(makeCourse());
    app.button1Short();
    const ClubPickerText t = clubPickerText(app);
    TEST_ASSERT_EQUAL_STRING("", t.subtitle);
    TEST_ASSERT_EQUAL_STRING("Driver vorgeschlagen", t.hint);
}

static void test_longest_club_name_fits() {
    App app(makeCourse());
    app.button1Short();
    for (int i = 0; i < kClubCount; i++) {
        const ClubPickerText t = clubPickerText(app);
        char expected[48];
        std::snprintf(expected, sizeof(expected), "%s vorgeschlagen", clubName(app.suggestedClub()));
        TEST_ASSERT_EQUAL_STRING(expected, t.hint);
        app.button2Short();
    }
}

static void test_menu_texts() {
    TEST_ASSERT_EQUAL_STRING("Grün hier markieren", menuItemText(MenuItem::MarkGreen));
    TEST_ASSERT_EQUAL_STRING("Vorheriges Loch", menuItemText(MenuItem::PreviousHole));
    TEST_ASSERT_EQUAL_STRING("Runde beenden", menuItemText(MenuItem::EndRound));
    TEST_ASSERT_EQUAL_STRING("Schliessen", menuItemText(MenuItem::Close));
}

// ---------- Loch fertig ----------

static HoleDoneText finishWith(App& app, int strokes, int putts) {
    for (int i = 0; i < strokes; i++) {
        app.button1Short();
        app.selectClub(i >= strokes - putts ? Club::Putter : Club::Iron7);
    }
    app.button2Short();
    return holeDoneText(app);
}

static void test_hole_done_names_and_tones() {
    struct Case {
        int strokes;
        const char* name;
        ScoreTone tone;
    };
    const Case cases[] = {{1, "Hole-in-one", ScoreTone::Under}, {2, "Albatros", ScoreTone::Under},
                          {3, "Eagle", ScoreTone::Under},       {4, "Birdie", ScoreTone::Under},
                          {5, "Par", ScoreTone::Par},           {6, "Bogey", ScoreTone::Over},
                          {7, "Doppelbogey", ScoreTone::Over},  {8, "+3", ScoreTone::Over},
                          {12, "+7", ScoreTone::Over}};
    for (const Case& c : cases) {
        App app(makeCourse()); // Loch 1 ist ein Par 5
        const HoleDoneText t = finishWith(app, c.strokes, 0);
        TEST_ASSERT_EQUAL_STRING(c.name, t.name);
        TEST_ASSERT_TRUE_MESSAGE(t.tone == c.tone, c.name);
        char strokes[8];
        std::snprintf(strokes, sizeof(strokes), "%d", c.strokes);
        TEST_ASSERT_EQUAL_STRING(strokes, t.strokes);
    }
}

static void test_hole_done_caption_putts_and_hint() {
    App app(makeCourse());
    HoleDoneText t = finishWith(app, 6, 2);
    TEST_ASSERT_EQUAL_STRING("LOCH 1 · PAR 5", t.caption);
    TEST_ASSERT_EQUAL_STRING("2 Putts", t.putts);
    TEST_ASSERT_EQUAL_STRING("Knopf drücken für Loch 2", t.hint);
    app.button2Short();
    t = finishWith(app, 3, 1);
    TEST_ASSERT_EQUAL_STRING("LOCH 2 · PAR 3", t.caption);
    TEST_ASSERT_EQUAL_STRING("1 Putt", t.putts);
    app.button2Short();
    t = finishWith(app, 4, 0); // Loch 3 ohne Par
    TEST_ASSERT_EQUAL_STRING("LOCH 3", t.caption);
    TEST_ASSERT_EQUAL_STRING("", t.name);
    TEST_ASSERT_TRUE(t.tone == ScoreTone::Par);
    TEST_ASSERT_EQUAL_STRING("0 Putts", t.putts);
}

static void test_hole_done_on_last_hole_points_to_summary() {
    Course c = makeCourse();
    c.hole_count = 1;
    App app(c);
    TEST_ASSERT_EQUAL_STRING("Knopf drücken für die Rundenübersicht", finishWith(app, 5, 2).hint);
}

// ---------- Rundenuebersicht ----------

static void test_round_summary_of_empty_round() {
    App app(makeCourse());
    const RoundSummaryText t = roundSummaryText(app);
    TEST_ASSERT_EQUAL_STRING("0", t.strokes);
    TEST_ASSERT_EQUAL_STRING("Noch kein Loch gespielt", t.line);
    TEST_ASSERT_EQUAL_STRING("0 Putts", t.detail);
    TEST_ASSERT_EQUAL_STRING("Knopf 1 lang: neue Runde · Knopf 2: zurück", t.hint);
}

static void test_round_summary_with_scores() {
    App app(makeCourse());
    hit(app, 0.0);
    hit(app, 244.0);
    app.button1Short();
    app.selectClub(Club::Putter);
    app.button1Short();
    app.selectClub(Club::Putter);
    app.button1Short();
    app.selectClub(Club::Putter);
    app.button1Short();
    app.selectClub(Club::Putter);
    app.button2Short(); // 6 Schlaege auf Par 5
    RoundSummaryText t = roundSummaryText(app);
    TEST_ASSERT_EQUAL_STRING("6", t.strokes);
    TEST_ASSERT_EQUAL_STRING("+1 nach 1 Loch", t.line);
    TEST_ASSERT_TRUE(t.tone == ScoreTone::Over);
    TEST_ASSERT_EQUAL_STRING("4 Putts · längster Schlag 244 m", t.detail);

    app.button2Short();
    playHole(app, 2, 1); // Par 3: Birdie
    t = roundSummaryText(app);
    TEST_ASSERT_EQUAL_STRING("8", t.strokes);
    TEST_ASSERT_EQUAL_STRING("E nach 2 Löchern", t.line);
    TEST_ASSERT_TRUE(t.tone == ScoreTone::Par);
}

static void test_round_summary_under_par_and_without_par() {
    App a(makeCourse());
    playHole(a, 3, 1);
    RoundSummaryText t = roundSummaryText(a);
    TEST_ASSERT_EQUAL_STRING("-2 nach 1 Loch", t.line);
    TEST_ASSERT_TRUE(t.tone == ScoreTone::Under);
    TEST_ASSERT_EQUAL_STRING("1 Putt", t.detail);

    Course c;
    App b(c); // Platz ganz ohne Par
    playHole(b, 4, 2);
    playHole(b, 5, 2);
    t = roundSummaryText(b);
    TEST_ASSERT_EQUAL_STRING("2 Löcher", t.line);
    TEST_ASSERT_TRUE(t.tone == ScoreTone::Par);
}

static void test_longest_possible_texts_fit_their_buffers() {
    // 18 Loecher mit je 20 Schlaegen und sehr weiten Schlaegen.
    Course c = makeCourse();
    for (int h = 0; h < 18; h++) c.holes[h].par = 3;
    App app(c);
    for (int h = 0; h < 18; h++) {
        for (int i = 0; i < kMaxShotsPerHole; i++) {
            app.setPosition(offset(kTee, i * 5000.0));
            app.button1Short();
            app.selectClub(i > 9 ? Club::Putter : Club::PitchingWedge);
        }
        app.button2Short();
        if (h < 17) app.button2Short();
    }
    const RoundSummaryText s = roundSummaryText(app);
    TEST_ASSERT_EQUAL_STRING("360", s.strokes);
    TEST_ASSERT_EQUAL_STRING("+306 nach 18 Löchern", s.line);
    TEST_ASSERT_EQUAL_STRING("180 Putts · längster Schlag 999 m", s.detail);
    const ScorecardText k = scorecardText(app);
    TEST_ASSERT_EQUAL_STRING("18 Löcher gespielt", k.subtitle);
    TEST_ASSERT_EQUAL_STRING("360 Schläge", k.strokes);
    TEST_ASSERT_EQUAL_STRING("+306", k.to_par);
    TEST_ASSERT_EQUAL_STRING("180 Putts", k.putts);
    const HoleDoneText d = holeDoneText(app);
    TEST_ASSERT_EQUAL_STRING("LOCH 18 · PAR 3", d.caption);
    TEST_ASSERT_EQUAL_STRING("+17", d.name);
    TEST_ASSERT_EQUAL_STRING("10 Putts", d.putts);
    const ShotsScreenText sh = shotsScreenText(app);
    TEST_ASSERT_EQUAL_STRING("Pitching Wedge", sh.rows[0].club);
    TEST_ASSERT_EQUAL_STRING("999 m", sh.rows[0].distance);
}

// ---------- Rueckmeldungen ----------

static void test_every_message_has_a_text() {
    TEST_ASSERT_EQUAL_STRING("", messageText(Message::None));
    const Message all[] = {Message::ShotRemoved, Message::NoShotToRemove, Message::NoShotYet,   Message::HoleFull,
                           Message::GreenMarked, Message::NoGpsForGreen,  Message::AlreadyFirstHole,
                           Message::NewRoundStarted};
    for (Message m : all) {
        const size_t length = std::strlen(messageText(m));
        TEST_ASSERT_TRUE(length > 5);
        TEST_ASSERT_TRUE(length <= 32); // passt in die Einblendung
    }
    TEST_ASSERT_EQUAL_STRING("Letzter Schlag gelöscht", messageText(Message::ShotRemoved));
    TEST_ASSERT_EQUAL_STRING("Grünmitte gespeichert", messageText(Message::GreenMarked));
}

// ---------- Mit Handy verbinden ----------

static NetStatus net(NetMode mode, const char* ssid, const char* address) {
    NetStatus status;
    status.mode = mode;
    std::snprintf(status.ssid, sizeof(status.ssid), "%s", ssid);
    std::snprintf(status.address, sizeof(status.address), "%s", address);
    return status;
}

static void test_phone_text_while_starting() {
    App app(makeCourse());
    const PhoneText t = phoneText(app);
    TEST_ASSERT_EQUAL_STRING("WLAN wird gestartet ...", t.line1);
    TEST_ASSERT_EQUAL_STRING("", t.value1);
    TEST_ASSERT_EQUAL_STRING("", t.value2);
    TEST_ASSERT_EQUAL_STRING("Knopf drücken zum Beenden", t.hint);
}

static void test_phone_text_hotspot() {
    App app(makeCourse());
    app.setNetStatus(net(NetMode::Hotspot, "Golf-Tracker", "192.168.4.1"));
    const PhoneText t = phoneText(app);
    TEST_ASSERT_EQUAL_STRING("Am Handy dieses WLAN wählen:", t.line1);
    TEST_ASSERT_EQUAL_STRING("Golf-Tracker", t.value1);
    TEST_ASSERT_EQUAL_STRING("", t.detail1); // offener Hotspot: kein Passwort
    TEST_ASSERT_EQUAL_STRING("Dann im Browser öffnen:", t.line2);
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", t.value2);
}

static void test_phone_text_hotspot_shows_its_password_only_in_hotspot_mode() {
    App app(makeCourse());
    NetStatus status = net(NetMode::Hotspot, "Golf-Tracker", "192.168.4.1");
    std::snprintf(status.hotspot_password, sizeof(status.hotspot_password), "48213975");
    app.setNetStatus(status);
    TEST_ASSERT_EQUAL_STRING("Passwort 48213975", phoneText(app).detail1);
    status.mode = NetMode::Connected;
    app.setNetStatus(status);
    TEST_ASSERT_EQUAL_STRING("", phoneText(app).detail1);
}

static void test_phone_text_connecting_and_connected() {
    App app(makeCourse());
    app.setNetStatus(net(NetMode::Connecting, "Heimnetz", ""));
    PhoneText t = phoneText(app);
    TEST_ASSERT_EQUAL_STRING("Verbinde mit WLAN", t.line1);
    TEST_ASSERT_EQUAL_STRING("Heimnetz", t.value1);
    TEST_ASSERT_EQUAL_STRING("", t.line2);

    app.setNetStatus(net(NetMode::Connected, "Heimnetz", "192.168.1.57"));
    t = phoneText(app);
    TEST_ASSERT_EQUAL_STRING("Verbunden mit WLAN", t.line1);
    TEST_ASSERT_EQUAL_STRING("Am Handy im Browser öffnen:", t.line2);
    TEST_ASSERT_EQUAL_STRING("192.168.1.57", t.value2);
}

static void test_phone_text_with_longest_names() {
    App app(makeCourse());
    app.setNetStatus(net(NetMode::Connected, "12345678901234567890123456789012", "fe80:0000:0000:0000:0202:b3ff:fe1e:8329"));
    const PhoneText t = phoneText(app);
    TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012", t.value1); // 32 Zeichen
    TEST_ASSERT_EQUAL_STRING("fe80:0000:0000:0000:0202:b3ff:fe1e:8329", t.value2);
}

static void test_menu_has_phone_entry() {
    TEST_ASSERT_EQUAL_STRING("Mit Handy verbinden", menuItemText(MenuItem::Phone));
    TEST_ASSERT_EQUAL_INT(5, kMenuItemCount);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tee_of_first_hole);
    RUN_TEST(test_as_in_the_prototype_after_the_drive);
    RUN_TEST(test_plural_and_putt_without_distance);
    RUN_TEST(test_no_gps);
    RUN_TEST(test_shot_counted_without_gps_shows_no_distance);
    RUN_TEST(test_hole_without_green);
    RUN_TEST(test_no_gps_wins_over_missing_green);
    RUN_TEST(test_hole_without_par_and_tee);
    RUN_TEST(test_length_without_par);
    RUN_TEST(test_hole_18_title_fits);
    RUN_TEST(test_far_away_is_capped_at_999);
    RUN_TEST(test_on_the_green);
    RUN_TEST(test_twenty_shots_fit);
    RUN_TEST(test_shots_view_empty);
    RUN_TEST(test_shots_view_lists_clubs_and_distances);
    RUN_TEST(test_shots_view_last_shot_is_live_and_open_club_is_a_dash);
    RUN_TEST(test_shots_view_without_gps_has_empty_distances);
    RUN_TEST(test_shots_view_with_twenty_shots);
    RUN_TEST(test_scorecard_of_new_round);
    RUN_TEST(test_scorecard_colours_and_totals);
    RUN_TEST(test_scorecard_singular_and_even_par);
    RUN_TEST(test_scorecard_under_par_and_hole_without_par);
    RUN_TEST(test_scorecard_nine_holes_and_two_digit_scores);
    RUN_TEST(test_club_picker_text);
    RUN_TEST(test_club_picker_shows_only_the_clubs_in_the_bag);
    RUN_TEST(test_club_picker_with_a_full_bag_of_14);
    RUN_TEST(test_club_picker_without_distance);
    RUN_TEST(test_longest_club_name_fits);
    RUN_TEST(test_menu_texts);
    RUN_TEST(test_hole_done_names_and_tones);
    RUN_TEST(test_hole_done_caption_putts_and_hint);
    RUN_TEST(test_hole_done_on_last_hole_points_to_summary);
    RUN_TEST(test_round_summary_of_empty_round);
    RUN_TEST(test_round_summary_with_scores);
    RUN_TEST(test_round_summary_under_par_and_without_par);
    RUN_TEST(test_longest_possible_texts_fit_their_buffers);
    RUN_TEST(test_every_message_has_a_text);
    RUN_TEST(test_phone_text_while_starting);
    RUN_TEST(test_phone_text_hotspot);
    RUN_TEST(test_phone_text_hotspot_shows_its_password_only_in_hotspot_mode);
    RUN_TEST(test_phone_text_connecting_and_connected);
    RUN_TEST(test_phone_text_with_longest_names);
    RUN_TEST(test_menu_has_phone_entry);
    return UNITY_END();
}
