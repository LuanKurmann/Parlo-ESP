#include <cstdio>
#include <cstring>
#include <string>

#include <unity.h>

#include "core/nmea.h"

using golf::NmeaParser;

void setUp() {}
void tearDown() {}

// Baut aus dem Satzinhalt einen vollstaendigen Satz mit korrekter Pruefsumme.
static std::string sentence(const std::string& body) {
    unsigned char sum = 0;
    for (char c : body) sum ^= static_cast<unsigned char>(c);
    char tail[8];
    std::snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
    return "$" + body + tail;
}

static bool feed(NmeaParser& p, const std::string& text) { return p.feed(text.data(), text.size()); }

// Zwei bekannte Beispielsaetze aus der NMEA-Literatur, Pruefsumme wie veroeffentlicht.
static const char* kGgaExample = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
static const char* kRmcExample = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

static void test_starts_without_fix() {
    NmeaParser p;
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_FALSE(p.fix().has_time);
    TEST_ASSERT_FALSE(p.fix().has_date);
}

static void test_gga_example() {
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, kGgaExample));
    const golf::GpsFix& f = p.fix();
    TEST_ASSERT_TRUE(f.valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 48.0 + 7.038 / 60.0, f.position.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 11.0 + 31.0 / 60.0, f.position.lon);
    TEST_ASSERT_EQUAL_INT(8, f.satellites);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, f.hdop);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 545.4f, f.altitude_m);
    TEST_ASSERT_TRUE(f.has_time);
    TEST_ASSERT_EQUAL_INT(12, f.hour);
    TEST_ASSERT_EQUAL_INT(35, f.minute);
    TEST_ASSERT_EQUAL_INT(19, f.second);
    TEST_ASSERT_EQUAL_UINT32(1, p.acceptedSentences());
}

static void test_rmc_example() {
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, kRmcExample));
    const golf::GpsFix& f = p.fix();
    TEST_ASSERT_TRUE(f.valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 48.1173, f.position.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 11.5166667, f.position.lon);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.4f * 1.852f, f.speed_kmh);
    TEST_ASSERT_TRUE(f.has_date);
    TEST_ASSERT_EQUAL_INT(23, f.day);
    TEST_ASSERT_EQUAL_INT(3, f.month);
    TEST_ASSERT_EQUAL_INT(2094, f.year);
}

static void test_multi_constellation_talker_and_fractional_seconds() {
    // So sendet ein moderner Empfaenger wie der LC76G.
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, sentence("GNGGA,081530.000,4722.6140,N,00832.5020,E,1,17,0.72,431.2,M,47.3,M,,")));
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 47.0 + 22.6140 / 60.0, p.fix().position.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 8.0 + 32.5020 / 60.0, p.fix().position.lon);
    TEST_ASSERT_EQUAL_INT(17, p.fix().satellites);
    TEST_ASSERT_EQUAL_INT(8, p.fix().hour);
    TEST_ASSERT_EQUAL_INT(30, p.fix().second);

    TEST_ASSERT_TRUE(feed(p, sentence("GNRMC,081531.000,A,4722.6141,N,00832.5021,E,2.50,90.0,231126,,,A,V")));
    TEST_ASSERT_EQUAL_INT(2026, p.fix().year);
    TEST_ASSERT_EQUAL_INT(11, p.fix().month);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.63f, p.fix().speed_kmh);
}

static void test_south_and_west_are_negative() {
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, sentence("GPGGA,010203,3353.4000,S,07030.0000,W,1,09,1.1,20.0,M,0.0,M,,")));
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -(33.0 + 53.4 / 60.0), p.fix().position.lat);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, -70.5, p.fix().position.lon);
}

static void test_negative_altitude() {
    NmeaParser p;
    feed(p, sentence("GPGGA,010203,3130.0000,N,03530.0000,E,1,09,1.1,-412.5,M,0.0,M,,"));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -412.5f, p.fix().altitude_m);
}

static void test_wrong_checksum_is_rejected() {
    NmeaParser p;
    TEST_ASSERT_FALSE(feed(p, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48\r\n"));
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_EQUAL_UINT32(0, p.acceptedSentences());
    TEST_ASSERT_EQUAL_UINT32(1, p.rejectedSentences());
}

static void test_single_flipped_character_is_rejected() {
    // Ein Uebertragungsfehler in der Breite darf nie als Position durchgehen.
    NmeaParser p;
    TEST_ASSERT_FALSE(feed(p, "$GPGGA,123519,4907.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"));
    TEST_ASSERT_FALSE(p.fix().valid);
}

static void test_missing_checksum_is_rejected() {
    NmeaParser p;
    TEST_ASSERT_FALSE(feed(p, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,\r\n"));
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_EQUAL_UINT32(1, p.rejectedSentences());
}

static void test_lowercase_checksum_is_accepted() {
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a\r\n"));
    TEST_ASSERT_TRUE(p.fix().valid);
}

static void test_no_fix_yet_gga_with_empty_fields() {
    // Typische Ausgabe direkt nach dem Einschalten.
    NmeaParser p;
    TEST_ASSERT_TRUE(feed(p, sentence("GNGGA,,,,,,0,00,99.99,,,,,,")));
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_EQUAL_INT(0, p.fix().satellites);
    TEST_ASSERT_FALSE(p.fix().has_time);
}

static void test_fix_is_lost_when_quality_drops_to_zero() {
    NmeaParser p;
    feed(p, kGgaExample);
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_TRUE(feed(p, sentence("GPGGA,123520,,,,,0,03,9.9,,,,,,")));
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_EQUAL_INT(3, p.fix().satellites);
}

static void test_rmc_void_status_invalidates_fix_but_keeps_date() {
    NmeaParser p;
    feed(p, kRmcExample);
    TEST_ASSERT_TRUE(feed(p, sentence("GNRMC,081531.000,V,,,,,,,231126,,,N,V")));
    TEST_ASSERT_FALSE(p.fix().valid);
    TEST_ASSERT_EQUAL_INT(2026, p.fix().year);
}

static void test_fix_returns_after_loss() {
    NmeaParser p;
    feed(p, kGgaExample);
    feed(p, sentence("GPGGA,123520,,,,,0,03,9.9,,,,,,"));
    feed(p, kGgaExample);
    TEST_ASSERT_TRUE(p.fix().valid);
}

static void test_other_sentences_are_ignored() {
    NmeaParser p;
    feed(p, kGgaExample);
    TEST_ASSERT_FALSE(feed(p, sentence("GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00")));
    TEST_ASSERT_FALSE(feed(p, sentence("GNGSA,A,3,04,05,09,12,,,,,,,,,1.8,1.0,1.5,1")));
    TEST_ASSERT_FALSE(feed(p, sentence("GNVTG,84.4,T,,M,22.4,N,41.5,K,A")));
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_EQUAL_UINT32(4, p.acceptedSentences());
    TEST_ASSERT_EQUAL_UINT32(0, p.rejectedSentences());
}

static void test_invalid_coordinates_never_become_a_fix() {
    const char* bodies[] = {
        "GPGGA,123519,9100.000,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",  // Breite ueber 90
        "GPGGA,123519,4860.000,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",  // 60 Minuten
        "GPGGA,123519,4807.038,N,18100.000,E,1,08,0.9,545.4,M,46.9,M,,",  // Laenge ueber 180
        "GPGGA,123519,4807.038,X,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",  // falsche Himmelsrichtung
        "GPGGA,123519,4807.038,N,01131.000,,1,08,0.9,545.4,M,46.9,M,,",   // fehlende Himmelsrichtung
        "GPGGA,123519,48a7.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",  // Buchstabe in der Zahl
        "GPGGA,123519,807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",   // zu wenig Ziffern
        "GPGGA,123519,4807.038,N,1131.000,E,1,08,0.9,545.4,M,46.9,M,,",   // Laenge zu kurz
        "GPGGA,123519,4807.03.8,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", // zwei Punkte
        "GPRMC,123519,A,4807.038,N,,E,022.4,084.4,230394,003.1,W",        // leere Laenge
    };
    for (const char* body : bodies) {
        NmeaParser p;
        feed(p, kGgaExample); // vorher gueltig
        feed(p, sentence(body));
        TEST_ASSERT_FALSE_MESSAGE(p.fix().valid, body);
    }
}

static void test_truncated_sentences_do_not_change_anything() {
    NmeaParser p;
    TEST_ASSERT_FALSE(feed(p, sentence("GPGGA,123519,4807.038,N")));
    TEST_ASSERT_FALSE(feed(p, sentence("GPRMC,123519,A")));
    TEST_ASSERT_FALSE(feed(p, sentence("GPGGA,123519,4807.038,N,01131.000,E,x,08,0.9,545.4,M,46.9,M,,")));
    TEST_ASSERT_FALSE(feed(p, sentence("GPRMC,123519,X,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W")));
    TEST_ASSERT_FALSE(p.fix().valid);
}

static void test_invalid_time_and_date_are_not_taken() {
    NmeaParser p;
    feed(p, sentence("GPRMC,256199,A,4807.038,N,01131.000,E,022.4,084.4,321394,003.1,W"));
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_FALSE(p.fix().has_time);
    TEST_ASSERT_FALSE(p.fix().has_date);
}

static void test_sentence_split_across_many_reads() {
    // Die serielle Schnittstelle liefert Saetze in beliebigen Stuecken.
    NmeaParser p;
    const std::string s = kGgaExample;
    int changes = 0;
    for (size_t i = 0; i < s.size(); i += 7) {
        if (feed(p, s.substr(i, 7))) changes++;
    }
    TEST_ASSERT_EQUAL_INT(1, changes);
    TEST_ASSERT_TRUE(p.fix().valid);
}

static void test_garbage_between_sentences() {
    NmeaParser p;
    std::string stream = "\xff\x00garbage,*12\r\n";
    stream += kGgaExample;
    stream += "noise without dollar\r\n\r\n\n";
    stream += kRmcExample;
    TEST_ASSERT_TRUE(p.feed(stream.data(), stream.size()));
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_EQUAL_UINT32(2, p.acceptedSentences());
}

static void test_new_dollar_restarts_a_broken_sentence() {
    // Satzanfang ging verloren, der naechste Satz beginnt mitten in der Zeile.
    NmeaParser p;
    std::string stream = "$GPGGA,123519,4807.0";
    stream += kRmcExample;
    TEST_ASSERT_TRUE(feed(p, stream));
    TEST_ASSERT_TRUE(p.fix().valid);
    TEST_ASSERT_TRUE(p.fix().has_date);
}

static void test_overlong_line_is_dropped_and_parser_recovers() {
    NmeaParser p;
    std::string stream = "$GPGGA," + std::string(500, '7') + "*00\r\n";
    TEST_ASSERT_FALSE(feed(p, stream));
    TEST_ASSERT_EQUAL_UINT32(1, p.rejectedSentences());
    TEST_ASSERT_TRUE(feed(p, kGgaExample));
    TEST_ASSERT_TRUE(p.fix().valid);
}

static void test_too_many_fields_are_rejected() {
    NmeaParser p;
    TEST_ASSERT_FALSE(feed(p, sentence("GPGGA" + std::string(40, ','))));
    TEST_ASSERT_FALSE(p.fix().valid);
}

static void test_random_bytes_never_produce_a_fix() {
    // Fester Startwert, damit der Test jedes Mal dasselbe prueft.
    NmeaParser p;
    uint32_t state = 12345;
    const char alphabet[] = "$GPNGARMC,.*0123456789ABCDEFNSEWV\r\n";
    for (int i = 0; i < 200000; i++) {
        state = state * 1664525u + 1013904223u;
        const uint32_t r = state >> 16;
        // Meist Zeichen, die in NMEA vorkommen, dazwischen beliebige Bytes.
        const char c = (r % 10 == 0) ? static_cast<char>(r >> 4) : alphabet[r % (sizeof(alphabet) - 1)];
        p.feed(c);
    }
    TEST_ASSERT_FALSE(p.fix().valid);
    // Danach muss ein normaler Satz wieder funktionieren.
    TEST_ASSERT_TRUE(feed(p, std::string("\r\n") + kGgaExample));
    TEST_ASSERT_TRUE(p.fix().valid);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_starts_without_fix);
    RUN_TEST(test_gga_example);
    RUN_TEST(test_rmc_example);
    RUN_TEST(test_multi_constellation_talker_and_fractional_seconds);
    RUN_TEST(test_south_and_west_are_negative);
    RUN_TEST(test_negative_altitude);
    RUN_TEST(test_wrong_checksum_is_rejected);
    RUN_TEST(test_single_flipped_character_is_rejected);
    RUN_TEST(test_missing_checksum_is_rejected);
    RUN_TEST(test_lowercase_checksum_is_accepted);
    RUN_TEST(test_no_fix_yet_gga_with_empty_fields);
    RUN_TEST(test_fix_is_lost_when_quality_drops_to_zero);
    RUN_TEST(test_rmc_void_status_invalidates_fix_but_keeps_date);
    RUN_TEST(test_fix_returns_after_loss);
    RUN_TEST(test_other_sentences_are_ignored);
    RUN_TEST(test_invalid_coordinates_never_become_a_fix);
    RUN_TEST(test_truncated_sentences_do_not_change_anything);
    RUN_TEST(test_invalid_time_and_date_are_not_taken);
    RUN_TEST(test_sentence_split_across_many_reads);
    RUN_TEST(test_garbage_between_sentences);
    RUN_TEST(test_new_dollar_restarts_a_broken_sentence);
    RUN_TEST(test_overlong_line_is_dropped_and_parser_recovers);
    RUN_TEST(test_too_many_fields_are_rejected);
    RUN_TEST(test_random_bytes_never_produce_a_fix);
    return UNITY_END();
}
