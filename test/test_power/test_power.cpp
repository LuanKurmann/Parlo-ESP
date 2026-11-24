#include <unity.h>

#include "../memory_fs.h"
#include "../scenario.h"
#include "core/display_text.h"
#include "core/power.h"

using namespace golf;
using testsupport::MemoryFs;

void setUp() {}
void tearDown() {}

#define ASSERT_STATE(expected, power) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>((power).state()))

// Laesst die Zeit im 100-ms-Takt bis `until_ms` laufen.
static void idle(ScreenPower& power, uint32_t& now, uint32_t until_ms, bool keep_on = false) {
    for (; now < until_ms; now += 100) power.tick(now, keep_on);
    power.tick(now, keep_on);
}

// ---------- Display ----------

static void test_starts_bright() {
    ScreenPower power;
    ASSERT_STATE(ScreenState::Active, power);
    TEST_ASSERT_EQUAL_UINT8(200, power.brightness());
}

static void test_dims_after_20_seconds_and_turns_off_after_30_minutes() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 19900);
    ASSERT_STATE(ScreenState::Active, power);
    idle(power, now, 20000);
    ASSERT_STATE(ScreenState::Dim, power);
    TEST_ASSERT_EQUAL_UINT8(40, power.brightness());
    idle(power, now, 1799900);
    ASSERT_STATE(ScreenState::Dim, power);
    idle(power, now, 1800000);
    ASSERT_STATE(ScreenState::Off, power);
    TEST_ASSERT_EQUAL_UINT8(0, power.brightness());
}

static void test_time_counts_from_first_tick_not_from_zero() {
    // Das Geraet ist schon laenger an, bevor die Stromsparlogik startet.
    ScreenPower power;
    uint32_t now = 1000000;
    power.tick(now, false);
    ASSERT_STATE(ScreenState::Active, power);
    idle(power, now, 1019900);
    ASSERT_STATE(ScreenState::Active, power);
}

static void test_input_while_bright_or_dim_passes_and_restarts_the_timer() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 15000);
    TEST_ASSERT_TRUE(power.onInput(now)); // hell: geht durch
    idle(power, now, 34900);
    ASSERT_STATE(ScreenState::Active, power); // 20 s zaehlen ab der Eingabe
    idle(power, now, 60000);
    ASSERT_STATE(ScreenState::Dim, power);
    TEST_ASSERT_TRUE(power.onInput(now)); // gedimmt: geht durch und macht hell
    ASSERT_STATE(ScreenState::Active, power);
}

static void test_first_input_while_off_only_wakes() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 2000000);
    ASSERT_STATE(ScreenState::Off, power);
    TEST_ASSERT_FALSE(power.onInput(now)); // weckt nur
    ASSERT_STATE(ScreenState::Active, power);
    TEST_ASSERT_TRUE(power.onInput(now + 500)); // die zweite Eingabe wirkt
}

static void test_keep_on_never_turns_off_but_still_dims() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 4 * 3600000, true); // vier Stunden "Mit Handy verbinden"
    ASSERT_STATE(ScreenState::Dim, power);
    idle(power, now, 4 * 3600000 + 1790000, false);
    ASSERT_STATE(ScreenState::Off, power); // danach geht es normal aus
}

static void test_keep_on_brings_an_off_screen_back_to_dim() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 2000000);
    ASSERT_STATE(ScreenState::Off, power);
    power.tick(now, true);
    ASSERT_STATE(ScreenState::Dim, power);
}

static void test_off_stays_off_across_timer_overflow() {
    ScreenPower power;
    uint32_t now = 0xFFF00000u;
    power.tick(now, false);
    for (uint32_t i = 0; i < 20000; i++) { // gut 33 Minuten, ueber den Ueberlauf hinweg
        now += 100;
        power.tick(now, false);
    }
    ASSERT_STATE(ScreenState::Off, power);
    for (uint32_t i = 0; i < 200000; i++) { // weitere 5.5 Stunden
        now += 100;
        power.tick(now, false);
    }
    ASSERT_STATE(ScreenState::Off, power);
}

static void test_dim_with_keep_on_survives_many_timer_overflows() {
    ScreenPower power;
    uint32_t now = 0;
    power.tick(now, true);
    for (int step = 0; step < 300; step++) { // 300 Schritte von je einem halben Monat
        now += 0x50000000u;
        power.tick(now, true);
        ASSERT_STATE(ScreenState::Dim, power);
    }
}

static void test_custom_times() {
    ScreenPower power(1000, 5000);
    uint32_t now = 0;
    idle(power, now, 1000);
    ASSERT_STATE(ScreenState::Dim, power);
    idle(power, now, 5000);
    ASSERT_STATE(ScreenState::Off, power);
}

// ---------- Touch ----------

static void test_touch_while_on_passes_and_keeps_screen_bright() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 25000);
    ASSERT_STATE(ScreenState::Dim, power);
    TEST_ASSERT_TRUE(power.onTouch(true, now));
    ASSERT_STATE(ScreenState::Active, power);
    TEST_ASSERT_TRUE(power.onTouch(true, now + 50));
    TEST_ASSERT_FALSE(power.onTouch(false, now + 100)); // losgelassen
}

static void test_touch_that_wakes_is_swallowed_until_release() {
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 2000000);
    TEST_ASSERT_FALSE(power.onTouch(true, now)); // weckt
    ASSERT_STATE(ScreenState::Active, power);
    for (int i = 1; i < 20; i++) TEST_ASSERT_FALSE(power.onTouch(true, now + i * 30)); // Finger bleibt drauf
    TEST_ASSERT_FALSE(power.onTouch(false, now + 700));
    TEST_ASSERT_TRUE(power.onTouch(true, now + 900)); // neue Beruehrung wirkt
}

static void test_released_touch_does_not_count_as_input() {
    ScreenPower power;
    uint32_t now = 0;
    for (; now < 30000; now += 30) {
        power.onTouch(false, now); // so fragt LVGL den Touch laufend ab
        power.tick(now, false);
    }
    ASSERT_STATE(ScreenState::Dim, power);
}

// ---------- Zusammenspiel mit Knoepfen und App ----------

static void test_button_event_on_dark_screen_does_not_count_a_shot() {
    App app;
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 2000000);
    dispatchButton(app, power, 1, ButtonEvent::ShortPress, now); // weckt nur
    TEST_ASSERT_EQUAL_INT(0, app.round().shotCount(0));
    TEST_ASSERT_TRUE(app.overlay() == Overlay::None);
    dispatchButton(app, power, 1, ButtonEvent::ShortPress, now + 600);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
}

static void test_long_press_on_dark_screen_does_nothing_but_wake() {
    App app;
    ScreenPower power;
    uint32_t now = 0;
    app.button1Short();
    app.button1Short(); // ein Schlag ist erfasst
    idle(power, now, 2000000);
    dispatchButton(app, power, 1, ButtonEvent::LongPress, now); // wuerde sonst den Schlag loeschen
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    dispatchButton(app, power, 2, ButtonEvent::None, now); // kein Ereignis: weckt nicht
    idle(power, now, 4000000);
    dispatchButton(app, power, 2, ButtonEvent::None, now);
    ASSERT_STATE(ScreenState::Off, power);
}

static void test_dimmed_screen_counts_the_shot_at_once() {
    App app;
    ScreenPower power;
    uint32_t now = 0;
    idle(power, now, 60000);
    ASSERT_STATE(ScreenState::Dim, power);
    dispatchButton(app, power, 1, ButtonEvent::ShortPress, now);
    TEST_ASSERT_EQUAL_INT(1, app.round().shotCount(0));
    ASSERT_STATE(ScreenState::Active, power);
}

static void test_hole_played_with_screen_power_like_on_the_device() {
    // Ganzer Ablauf mit echten Knopfzeiten: Zwischen den Schlaegen geht man
    // minutenlang; das Display dimmt, zaehlt aber sofort.
    MemoryFs fs;
    Store(fs).saveCourse(demoCourse());
    testsupport::Scenario s(fs);
    const Hole hole = s.app().course().holes[0];
    s.placeAt(hole.tee);
    s.wait(3.0);
    s.press1();
    s.press1();
    s.walkTo(positionOnHole(hole, 205.0)); // rund zweieinhalb Minuten
    ASSERT_STATE(ScreenState::Dim, s.power());
    s.wait(2.0);
    s.press1();
    TEST_ASSERT_EQUAL_INT(2, s.app().round().shotCount(0)); // gedimmt: Schlag zaehlt sofort
    s.press1();

    s.wait(600.0); // zehn Minuten Stau am Abschlag: weiterhin nur gedimmt
    ASSERT_STATE(ScreenState::Dim, s.power());
    s.press1();
    TEST_ASSERT_EQUAL_INT(3, s.app().round().shotCount(0)); // zaehlt sofort
    s.press1();

    s.wait(2000.0); // Geraet nach der Runde vergessen: Display ganz aus
    ASSERT_STATE(ScreenState::Off, s.power());
    TEST_ASSERT_TRUE(s.app().hasPosition()); // GPS laeuft weiter
    s.press1(); // weckt nur
    TEST_ASSERT_EQUAL_INT(3, s.app().round().shotCount(0));
    ASSERT_STATE(ScreenState::Active, s.power());
    s.press1(); // jetzt zaehlt es
    TEST_ASSERT_EQUAL_INT(4, s.app().round().shotCount(0));
}

static void test_phone_screen_keeps_the_display_on() {
    MemoryFs fs;
    testsupport::Scenario s(fs);
    s.hold2();
    s.press2();
    s.press2();
    s.press2();
    s.press1(); // "Mit Handy verbinden"
    TEST_ASSERT_TRUE(s.app().phoneWanted());
    s.wait(3600.0); // eine Stunde am Handy
    ASSERT_STATE(ScreenState::Dim, s.power());
    s.press1(); // schliesst die Ansicht (gedimmt wirkt die Eingabe)
    TEST_ASSERT_FALSE(s.app().phoneWanted());
    s.wait(1810.0);
    ASSERT_STATE(ScreenState::Off, s.power());
}

// ---------- Akku ----------

static BatteryInfo battery(int percent, bool charging = false) {
    BatteryInfo b;
    b.known = true;
    b.percent = percent;
    b.charging = charging;
    return b;
}

static void test_battery_text() {
    App app;
    TEST_ASSERT_EQUAL_STRING("", batteryText(app).text); // unbekannt: keine Anzeige
    app.setBattery(battery(87));
    TEST_ASSERT_EQUAL_STRING("87 %", batteryText(app).text);
    TEST_ASSERT_FALSE(batteryText(app).low);
    app.setBattery(battery(20));
    TEST_ASSERT_TRUE(batteryText(app).low);
    app.setBattery(battery(21));
    TEST_ASSERT_FALSE(batteryText(app).low);
    app.setBattery(battery(12, true));
    TEST_ASSERT_EQUAL_STRING("lädt 12 %", batteryText(app).text);
    TEST_ASSERT_FALSE(batteryText(app).low); // am Laden wird nicht gewarnt
    app.setBattery(battery(100, true));
    TEST_ASSERT_EQUAL_STRING("lädt 100 %", batteryText(app).text);
}

static void test_battery_values_are_clamped() {
    App app;
    app.setBattery(battery(250));
    TEST_ASSERT_EQUAL_INT(100, app.battery().percent);
    app.setBattery(battery(-5));
    TEST_ASSERT_EQUAL_INT(0, app.battery().percent);
    TEST_ASSERT_EQUAL_STRING("0 %", batteryText(app).text);
}

static void test_low_battery_warns_once() {
    App app;
    app.setBattery(battery(40));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
    app.setBattery(battery(15));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::BatteryLow);
    for (int p = 15; p >= 0; p--) { // faellt weiter: keine neue Warnung
        app.setBattery(battery(p));
        TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
    }
    TEST_ASSERT_EQUAL_STRING("Akku fast leer", messageText(Message::BatteryLow));
}

static void test_low_battery_warns_again_after_charging() {
    App app;
    app.setBattery(battery(10));
    app.takeMessage();
    app.setBattery(battery(10, true)); // ans Ladegeraet
    app.setBattery(battery(60, true));
    app.setBattery(battery(60));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
    app.setBattery(battery(14));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::BatteryLow);
}

static void test_measurement_jitter_around_the_limit_does_not_repeat_the_warning() {
    App app;
    app.setBattery(battery(15));
    app.takeMessage();
    for (int i = 0; i < 50; i++) {
        app.setBattery(battery(15 + i % 4)); // 15..18 im Wechsel
        TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
    }
}

static void test_low_battery_does_not_replace_another_message() {
    App app;
    app.button1Long(); // "Kein Schlag zum Loeschen" steht an
    app.setBattery(battery(10));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::NoShotToRemove);
    app.setBattery(battery(10)); // beim naechsten Messwert kommt die Warnung
    TEST_ASSERT_TRUE(app.takeMessage() == Message::BatteryLow);
}

static void test_no_warning_while_charging_or_unknown() {
    App app;
    app.setBattery(battery(5, true));
    TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
    BatteryInfo unknown;
    unknown.percent = 3;
    app.setBattery(unknown);
    TEST_ASSERT_TRUE(app.takeMessage() == Message::None);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_bright);
    RUN_TEST(test_dims_after_20_seconds_and_turns_off_after_30_minutes);
    RUN_TEST(test_time_counts_from_first_tick_not_from_zero);
    RUN_TEST(test_input_while_bright_or_dim_passes_and_restarts_the_timer);
    RUN_TEST(test_first_input_while_off_only_wakes);
    RUN_TEST(test_keep_on_never_turns_off_but_still_dims);
    RUN_TEST(test_keep_on_brings_an_off_screen_back_to_dim);
    RUN_TEST(test_off_stays_off_across_timer_overflow);
    RUN_TEST(test_dim_with_keep_on_survives_many_timer_overflows);
    RUN_TEST(test_custom_times);
    RUN_TEST(test_touch_while_on_passes_and_keeps_screen_bright);
    RUN_TEST(test_touch_that_wakes_is_swallowed_until_release);
    RUN_TEST(test_released_touch_does_not_count_as_input);
    RUN_TEST(test_button_event_on_dark_screen_does_not_count_a_shot);
    RUN_TEST(test_long_press_on_dark_screen_does_nothing_but_wake);
    RUN_TEST(test_dimmed_screen_counts_the_shot_at_once);
    RUN_TEST(test_hole_played_with_screen_power_like_on_the_device);
    RUN_TEST(test_phone_screen_keeps_the_display_on);
    RUN_TEST(test_battery_text);
    RUN_TEST(test_battery_values_are_clamped);
    RUN_TEST(test_low_battery_warns_once);
    RUN_TEST(test_low_battery_warns_again_after_charging);
    RUN_TEST(test_measurement_jitter_around_the_limit_does_not_repeat_the_warning);
    RUN_TEST(test_low_battery_does_not_replace_another_message);
    RUN_TEST(test_no_warning_while_charging_or_unknown);
    return UNITY_END();
}
