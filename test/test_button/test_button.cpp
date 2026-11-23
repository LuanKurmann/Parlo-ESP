#include <unity.h>

#include "core/button.h"

using golf::Button;
using golf::ButtonEvent;

void setUp() {}
void tearDown() {}

// Haelt den Knopf von `from` bis `to` (Millisekunden) im Zustand `pressed`, in
// 5-ms-Schritten wie die Firmware. Zaehlt die Ereignisse.
struct Probe {
    Button button;
    int shorts = 0;
    int longs = 0;
    uint32_t first_long_at = 0;

    void hold(bool pressed, uint32_t from, uint32_t to) {
        // Differenz statt Vergleich, damit der Zaehler ueberlaufen darf.
        for (uint32_t elapsed = 0; elapsed < to - from; elapsed += 5) step(pressed, from + elapsed);
    }
    void step(bool pressed, uint32_t t) {
        const ButtonEvent e = button.update(pressed, t);
        if (e == ButtonEvent::ShortPress) shorts++;
        if (e == ButtonEvent::LongPress) {
            if (longs == 0) first_long_at = t;
            longs++;
        }
    }
};

static void test_nothing_happens_while_idle() {
    Probe p;
    p.hold(false, 0, 5000);
    TEST_ASSERT_EQUAL_INT(0, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
    TEST_ASSERT_FALSE(p.button.isDown());
}

static void test_short_press_fires_on_release() {
    Probe p;
    p.hold(false, 0, 100);
    p.hold(true, 100, 300);
    TEST_ASSERT_EQUAL_INT(0, p.shorts); // noch gedrueckt
    TEST_ASSERT_TRUE(p.button.isDown());
    p.hold(false, 300, 500);
    TEST_ASSERT_EQUAL_INT(1, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
}

static void test_long_press_fires_while_still_held() {
    Probe p;
    p.hold(false, 0, 100);
    p.hold(true, 100, 2000);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
    TEST_ASSERT_EQUAL_UINT32(650, p.first_long_at); // 100 + 550
    p.hold(false, 2000, 2500);
    TEST_ASSERT_EQUAL_INT(1, p.longs);  // nur einmal
    TEST_ASSERT_EQUAL_INT(0, p.shorts); // kein Kurz nach Lang
}

static void test_release_just_before_long_threshold_is_short() {
    Probe p;
    p.hold(true, 0, 545);
    p.hold(false, 545, 700);
    TEST_ASSERT_EQUAL_INT(1, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
}

static void test_glitch_shorter_than_debounce_is_ignored() {
    Probe p;
    p.hold(false, 0, 100);
    p.hold(true, 100, 120); // 20 ms Stoerung
    p.hold(false, 120, 500);
    TEST_ASSERT_EQUAL_INT(0, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
}

static void test_bouncing_contact_gives_one_short_press() {
    Probe p;
    // Prellen beim Druecken und beim Loslassen.
    const bool bounce_in[] = {true, false, true, false, true, true, false, true};
    uint32_t t = 100;
    for (bool b : bounce_in) p.step(b, t += 3);
    p.hold(true, 130, 300);
    const bool bounce_out[] = {false, true, false, true, false};
    t = 300;
    for (bool b : bounce_out) p.step(b, t += 3);
    p.hold(false, 320, 600);
    TEST_ASSERT_EQUAL_INT(1, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
}

static void test_brief_dropout_during_long_press_does_not_split_it() {
    Probe p;
    p.hold(true, 0, 300);
    p.hold(false, 300, 315); // 15 ms Aussetzer
    p.hold(true, 315, 1000);
    p.hold(false, 1000, 1200);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
    TEST_ASSERT_EQUAL_INT(0, p.shorts);
}

static void test_several_presses_in_a_row() {
    Probe p;
    uint32_t t = 0;
    for (int i = 0; i < 5; i++) {
        p.hold(true, t, t + 100);
        p.hold(false, t + 100, t + 200);
        t += 200;
    }
    TEST_ASSERT_EQUAL_INT(5, p.shorts);
    p.hold(true, t, t + 800);
    p.hold(false, t + 800, t + 900);
    p.hold(true, t + 900, t + 1000);
    p.hold(false, t + 1000, t + 1100);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
    TEST_ASSERT_EQUAL_INT(6, p.shorts);
}

static void test_works_across_timer_overflow() {
    // Der Millisekundenzaehler laeuft nach rund 49 Tagen ueber.
    Probe a;
    a.hold(false, 0xFFFFFF00u, 0xFFFFFF9Cu);
    a.hold(true, 0xFFFFFF9Cu, 0x00000064u); // kurzer Druck ueber den Ueberlauf
    a.hold(false, 0x00000064u, 0x000000C8u);
    TEST_ASSERT_EQUAL_INT(1, a.shorts);
    TEST_ASSERT_EQUAL_INT(0, a.longs);

    Probe b;
    b.hold(true, 0xFFFFFF00u, 0x00000400u); // langer Druck ueber den Ueberlauf
    b.hold(false, 0x00000400u, 0x00000500u);
    TEST_ASSERT_EQUAL_INT(1, b.longs);
    TEST_ASSERT_EQUAL_INT(0, b.shorts);
}

static void test_debounce_works_across_timer_overflow() {
    // Eine Stoerung genau am Ueberlauf darf nicht als Druck durchgehen.
    Probe p;
    p.hold(false, 0xFFFFFF00u, 0xFFFFFFF0u);
    p.hold(true, 0xFFFFFFF0u, 0xFFFFFFFAu); // 10 ms
    p.hold(false, 0xFFFFFFFAu, 0x000000C8u);
    TEST_ASSERT_EQUAL_INT(0, p.shorts);
    TEST_ASSERT_EQUAL_INT(0, p.longs);
}

static void test_button_held_at_power_on_counts_as_press() {
    Probe p;
    p.hold(true, 0, 1000);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
}

static void test_custom_timing() {
    Probe p;
    p.button = Button(1000, 50);
    p.hold(true, 0, 800);
    p.hold(false, 800, 1000);
    TEST_ASSERT_EQUAL_INT(1, p.shorts);
    p.hold(true, 1000, 2100);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
    TEST_ASSERT_EQUAL_UINT32(2000, p.first_long_at);
}

static void test_irregular_update_intervals() {
    // Auch wenn die Hauptschleife einmal haengt, darf kein Ereignis doppelt kommen.
    Probe p;
    p.step(false, 0);
    p.step(true, 10);
    p.step(true, 2000); // lange Pause mit gedruecktem Knopf
    p.step(true, 2005);
    p.step(false, 2010);
    p.step(false, 3000);
    TEST_ASSERT_EQUAL_INT(1, p.longs);
    TEST_ASSERT_EQUAL_INT(0, p.shorts);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_happens_while_idle);
    RUN_TEST(test_short_press_fires_on_release);
    RUN_TEST(test_long_press_fires_while_still_held);
    RUN_TEST(test_release_just_before_long_threshold_is_short);
    RUN_TEST(test_glitch_shorter_than_debounce_is_ignored);
    RUN_TEST(test_bouncing_contact_gives_one_short_press);
    RUN_TEST(test_brief_dropout_during_long_press_does_not_split_it);
    RUN_TEST(test_several_presses_in_a_row);
    RUN_TEST(test_works_across_timer_overflow);
    RUN_TEST(test_debounce_works_across_timer_overflow);
    RUN_TEST(test_button_held_at_power_on_counts_as_press);
    RUN_TEST(test_custom_timing);
    RUN_TEST(test_irregular_update_intervals);
    return UNITY_END();
}
