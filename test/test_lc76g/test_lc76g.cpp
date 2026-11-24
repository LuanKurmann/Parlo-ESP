#include <cstring>
#include <memory>
#include <string>

#include <unity.h>

#include "../memory_fs.h"
#include "core/demo_course.h"
#include "core/gps_feed.h"
#include "core/lc76g.h"
#include "core/nmea_writer.h"

using namespace golf;
using testsupport::MemoryFs;

void setUp() {}
void tearDown() {}

// Nachbau des GPS-Moduls am I2C-Bus. Haelt sich an den Ablauf des
// Hersteller-Beispiels und merkt sich jede Abweichung des Lesers davon.
class FakeLc76g : public I2cBus {
public:
    std::string pending;        // NMEA-Text, der im Modul bereitliegt
    uint32_t now_ms = 0;        // vom Test gesetzte Zeit
    bool bus_dead = false;      // jeder Zugriff schlaegt fehl
    int fail_next = 0;          // so viele naechste Zugriffe schlagen fehl
    uint32_t forced_length = 0; // != 0: diese Laenge melden (Unsinn simulieren)

    int violations = 0;         // Verstoesse gegen das Protokoll
    std::string last_violation;
    int writes = 0, reads = 0;

    bool write(uint8_t address, const uint8_t* data, size_t length) override {
        writes++;
        if (failing()) return false;
        if (address != 0x50) return violation("Schreiben an falsche Adresse");
        static const uint8_t ask[] = {0x08, 0x00, 0x51, 0xAA, 0x04, 0x00, 0x00, 0x00};
        if (length == 8 && std::memcmp(data, ask, 8) == 0) {
            offered_ = forced_length != 0 ? forced_length : static_cast<uint32_t>(pending.size());
            expect_ = Expect::LengthRead;
            last_write_ms_ = now_ms;
            return true;
        }
        static const uint8_t prefix[] = {0x00, 0x20, 0x51, 0xAA};
        if (length == 8 && std::memcmp(data, prefix, 4) == 0) {
            if (expect_ != Expect::DataRequest) return violation("Datenanforderung ohne vorherige Laenge");
            const uint32_t asked = data[4] | (data[5] << 8) | (data[6] << 16) | (static_cast<uint32_t>(data[7]) << 24);
            if (asked != offered_) return violation("angeforderte Laenge weicht von der gemeldeten ab");
            if (now_ms - last_read_ms_ < 100) return violation("Datenanforderung frueher als 100 ms nach dem Lesen");
            expect_ = Expect::DataRead;
            last_write_ms_ = now_ms;
            return true;
        }
        return violation("unbekanntes Kommando");
    }

    bool read(uint8_t address, uint8_t* data, size_t length) override {
        reads++;
        if (failing()) return false;
        if (address != 0x54) return violation("Lesen von falscher Adresse");
        if (now_ms - last_write_ms_ < 100) return violation("Lesen frueher als 100 ms nach dem Schreiben");
        if (expect_ == Expect::LengthRead) {
            if (length != 4) return violation("Laenge muss 4 Bytes sein");
            for (int i = 0; i < 4; i++) data[i] = static_cast<uint8_t>(offered_ >> (8 * i));
            expect_ = offered_ == 0 ? Expect::Nothing : Expect::DataRequest;
            last_read_ms_ = now_ms;
            return true;
        }
        if (expect_ == Expect::DataRead) {
            if (length != offered_) return violation("gelesene Laenge weicht ab");
            std::memcpy(data, pending.data(), length <= pending.size() ? length : pending.size());
            pending.erase(0, length);
            expect_ = Expect::Nothing;
            return true;
        }
        return violation("Lesen ohne vorheriges Kommando");
    }

private:
    enum class Expect { Nothing, LengthRead, DataRequest, DataRead };

    bool failing() {
        if (bus_dead) return true;
        if (fail_next > 0) {
            fail_next--;
            expect_ = Expect::Nothing;
            return true;
        }
        return false;
    }
    bool violation(const char* what) {
        violations++;
        last_violation = what;
        return false;
    }

    Expect expect_ = Expect::Nothing;
    uint32_t offered_ = 0;
    uint32_t last_write_ms_ = 0;
    uint32_t last_read_ms_ = 0;
};

// Laesst `ms` Millisekunden im 5-ms-Takt vergehen und sammelt den gelesenen Text.
static std::string run(Lc76gReader& reader, FakeLc76g& device, uint32_t ms) {
    std::string all, chunk;
    for (uint32_t end = device.now_ms + ms; device.now_ms != end; device.now_ms += 5) {
        if (reader.poll(device.now_ms, chunk)) all += chunk;
    }
    return all;
}

static NmeaFixInfo at(double metres_north, int second) {
    NmeaFixInfo info;
    info.position = positionOnHole(demoCourse().holes[0], metres_north);
    info.second = second % 60;
    info.minute = second / 60;
    return info;
}

static void test_reads_pending_text_following_the_protocol() {
    FakeLc76g device;
    Lc76gReader reader(device);
    device.pending = nmeaBurst(at(0.0, 0));
    const std::string expected = device.pending;
    const std::string got = run(reader, device, 1000);
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), got.c_str());
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, device.violations, device.last_violation.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, reader.errors());
    TEST_ASSERT_EQUAL_UINT32(1, reader.chunks());
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(device.pending.size()));
}

static void test_nothing_pending_keeps_asking_quietly() {
    FakeLc76g device;
    Lc76gReader reader(device);
    TEST_ASSERT_EQUAL_STRING("", run(reader, device, 5000).c_str());
    TEST_ASSERT_EQUAL_INT(0, device.violations);
    TEST_ASSERT_EQUAL_UINT32(0, reader.errors());
    TEST_ASSERT_TRUE(device.writes >= 10 && device.writes <= 15); // rund alle 400 ms
}

static void test_poll_never_blocks_and_does_one_step_at_most() {
    FakeLc76g device;
    Lc76gReader reader(device);
    device.pending = "$X*00\r\n";
    std::string chunk;
    reader.poll(0, chunk);
    TEST_ASSERT_EQUAL_INT(1, device.writes + device.reads);
    for (int i = 0; i < 1000; i++) reader.poll(0, chunk); // gleiche Zeit: nichts weiter
    TEST_ASSERT_EQUAL_INT(1, device.writes + device.reads);
}

static void test_continuous_stream_arrives_complete_and_in_order() {
    FakeLc76g device;
    Lc76gReader reader(device);
    std::string sent, got;
    for (int second = 0; second < 120; second++) {
        const std::string burst = nmeaBurst(at(second * 1.4, second));
        device.pending += burst; // das Modul sammelt, bis gelesen wird
        sent += burst;
        got += run(reader, device, 1000);
    }
    got += run(reader, device, 2000);
    TEST_ASSERT_TRUE(sent == got);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, device.violations, device.last_violation.c_str());
    TEST_ASSERT_TRUE(reader.chunks() >= 100); // jede Sekunde wird abgeholt, kein Stau
}

static void test_large_backlog_is_read_in_one_go() {
    FakeLc76g device;
    Lc76gReader reader(device);
    for (int second = 0; second < 20; second++) device.pending += nmeaBurst(at(0.0, second));
    const size_t size = device.pending.size();
    TEST_ASSERT_TRUE(size > 4000 && size <= Lc76gReader::kMaxChunkBytes);
    TEST_ASSERT_EQUAL_UINT32(size, run(reader, device, 1000).size());
}

static void test_nonsense_length_is_not_read() {
    FakeLc76g device;
    Lc76gReader reader(device);
    device.pending = "$X*00\r\n";
    device.forced_length = 0xFFFFFFFFu; // Bus liefert nur Einsen
    TEST_ASSERT_EQUAL_STRING("", run(reader, device, 3000).c_str());
    TEST_ASSERT_TRUE(reader.errors() >= 2);
    device.forced_length = Lc76gReader::kMaxChunkBytes + 1;
    run(reader, device, 2000);
    TEST_ASSERT_EQUAL_UINT32(0, reader.chunks());
    device.forced_length = 0;
    TEST_ASSERT_EQUAL_STRING("$X*00\r\n", run(reader, device, 3000).c_str());
}

static void test_bus_errors_back_off_and_recover() {
    FakeLc76g device;
    Lc76gReader reader(device);
    device.bus_dead = true; // Modul fehlt (Variante ohne GPS) oder Bus haengt
    run(reader, device, 10000);
    TEST_ASSERT_TRUE(reader.errors() >= 9 && reader.errors() <= 11); // ein Versuch pro Sekunde, kein Dauerfeuer
    TEST_ASSERT_TRUE(device.writes <= 11);

    device.bus_dead = false;
    device.pending = nmeaBurst(at(0.0, 0));
    const std::string expected = device.pending;
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), run(reader, device, 3000).c_str());
    TEST_ASSERT_EQUAL_INT(0, device.violations);
}

static void test_error_at_every_step_restarts_cleanly() {
    // Ein Fehler beim 1., 2., 3. oder 4. Buszugriff: Der Leser beginnt von vorn
    // und liest danach korrekt weiter, ohne das Protokoll zu verletzen.
    for (int failing_access = 1; failing_access <= 4; failing_access++) {
        FakeLc76g device;
        Lc76gReader reader(device);
        device.pending = nmeaBurst(at(0.0, 0));
        const std::string expected = device.pending;
        std::string chunk, got;
        int accesses = 0;
        for (; device.now_ms < 6000; device.now_ms += 5) {
            const int before = device.writes + device.reads;
            if (accesses + 1 == failing_access) device.fail_next = 1;
            if (reader.poll(device.now_ms, chunk)) got += chunk;
            if (device.writes + device.reads != before) accesses++;
            if (accesses >= failing_access) device.fail_next = 0;
        }
        TEST_ASSERT_EQUAL_STRING(expected.c_str(), got.c_str());
        TEST_ASSERT_EQUAL_UINT32(1, reader.errors());
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, device.violations, device.last_violation.c_str());
    }
}

static void test_works_across_timer_overflow() {
    FakeLc76g device;
    Lc76gReader reader(device);
    device.now_ms = 0xFFFFFE00u;
    std::string sent, got;
    for (int second = 0; second < 5; second++) {
        const std::string burst = nmeaBurst(at(0.0, second));
        device.pending += burst;
        sent += burst;
        got += run(reader, device, 1000);
    }
    got += run(reader, device, 1000);
    TEST_ASSERT_TRUE(sent == got);
    TEST_ASSERT_EQUAL_INT(0, device.violations);
}

static void test_i2c_gps_drives_the_app_end_to_end() {
    // Vom I2C-Bus bis zur Distanz in der App.
    MemoryFs fs;
    Store(fs).saveCourse(demoCourse());
    Session session(fs);
    GpsFeed feed(session);
    FakeLc76g device;
    Lc76gReader reader(device);

    std::string chunk;
    for (int second = 0; second < 150; second++) {
        device.pending += nmeaBurst(at(second * 1.4, second)); // 1.4 m/s Richtung Gruen
        for (int tick = 0; tick < 200; tick++, device.now_ms += 5) {
            if (reader.poll(device.now_ms, chunk)) feed.feed(chunk.data(), chunk.size(), device.now_ms);
            feed.tick(device.now_ms);
        }
    }
    TEST_ASSERT_TRUE(session.app().hasPosition());
    // Nach 150 s rund 209 m gegangen; die Anzeige darf hoechstens eine gute Sekunde zurueckliegen.
    TEST_ASSERT_INT_WITHIN(3, 342 - 209, session.app().distances().center);
    TEST_ASSERT_EQUAL_UINT32(0, feed.rejectedJumps());

    device.bus_dead = true; // Modul faellt aus
    for (int tick = 0; tick < 2000; tick++, device.now_ms += 5) {
        if (reader.poll(device.now_ms, chunk)) feed.feed(chunk.data(), chunk.size(), device.now_ms);
        feed.tick(device.now_ms);
    }
    TEST_ASSERT_FALSE(session.app().hasPosition());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reads_pending_text_following_the_protocol);
    RUN_TEST(test_nothing_pending_keeps_asking_quietly);
    RUN_TEST(test_poll_never_blocks_and_does_one_step_at_most);
    RUN_TEST(test_continuous_stream_arrives_complete_and_in_order);
    RUN_TEST(test_large_backlog_is_read_in_one_go);
    RUN_TEST(test_nonsense_length_is_not_read);
    RUN_TEST(test_bus_errors_back_off_and_recover);
    RUN_TEST(test_error_at_every_step_restarts_cleanly);
    RUN_TEST(test_works_across_timer_overflow);
    RUN_TEST(test_i2c_gps_drives_the_app_end_to_end);
    return UNITY_END();
}
