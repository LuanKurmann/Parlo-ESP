// Spielt ganze Ablaeufe so durch, wie das Geraet sie erlebt: NMEA-Zeichen vom
// (simulierten) GPS-Empfaenger und Knopfpegel im 5-ms-Takt. Dazwischen liegt
// der komplette Kern: Parser, Plausibilitaet, Knopf-Erkennung, App, Speicher.
#pragma once

#include <cmath>
#include <memory>
#include <string>

#include "core/button.h"
#include "core/demo_course.h"
#include "core/gps_feed.h"
#include "core/nmea_writer.h"
#include "core/power.h"
#include "core/session.h"
#include "memory_fs.h"

namespace testsupport {

class Scenario {
public:
    explicit Scenario(MemoryFs& fs, uint32_t seed = 1) : fs_(fs), rng_(seed) { boot(); }

    golf::App& app() { return session_->app(); }
    golf::Session& session() { return *session_; }
    golf::GpsFeed& feed() { return *feed_; }
    golf::ScreenPower& power() { return power_; }
    const golf::LatLon& truePosition() const { return true_position_; }

    // ---------- Umgebung ----------
    double noise_m = 0.0;        // Streuung der GPS-Position (Standardabweichung)
    bool has_fix = true;         // false: Empfaenger meldet "keine Position"
    bool receiver_on = true;     // false: Empfaenger sendet gar nichts
    bool has_time = true;        // false: Empfaenger kennt die Uhrzeit noch nicht
    double corrupt_rate = 0.0;   // Anteil zufaellig verfaelschter Zeichen
    int corrupted_bytes = 0;

    // Geraet aus und wieder ein (Daten im Speicher bleiben).
    void reboot() {
        fs_.reboot();
        boot();
    }

    void placeAt(const golf::LatLon& position) { true_position_ = position; }

    // ---------- Zeit vergehen lassen ----------

    // Laesst `ms` Millisekunden verstreichen; die Knoepfe haben dabei den
    // angegebenen Pegel. Jede volle Sekunde sendet der Empfaenger.
    void advance(uint32_t ms, bool button1 = false, bool button2 = false) {
        for (uint32_t elapsed = 0; elapsed < ms; elapsed += 5) {
            if (now_ms_ % 1000 == 0) emitBurst();
            // Wie in der Firmware: Ist das Display aus, weckt ein Knopf es nur.
            golf::dispatchButton(app(), power_, 1, b1_.update(button1, now_ms_), now_ms_);
            golf::dispatchButton(app(), power_, 2, b2_.update(button2, now_ms_), now_ms_);
            power_.tick(now_ms_, app().phoneWanted());
            feed_->tick(now_ms_);
            if (!session_->sync()) failed_syncs_++;
            now_ms_ += 5;
        }
    }

    void wait(double seconds) { advance(static_cast<uint32_t>(seconds * 1000.0)); }

    // Geht mit `speed` m/s zum Ziel.
    void walkTo(const golf::LatLon& target, double speed = 1.4) {
        for (;;) {
            const double remaining = golf::distanceMeters(true_position_, target);
            if (remaining < speed * 0.005) break;
            const double f = speed * 0.005 / remaining; // Schritt pro 5 ms
            true_position_.lat += (target.lat - true_position_.lat) * f;
            true_position_.lon += (target.lon - true_position_.lon) * f;
            advance(5);
        }
        true_position_ = target;
    }

    // ---------- Knoepfe ----------
    void press1() { press(true, false, 150); }
    void press2() { press(false, true, 150); }
    void hold1() { press(true, false, 900); }
    void hold2() { press(false, true, 900); }

    int failedSyncs() const { return failed_syncs_; }
    uint32_t nowMs() const { return now_ms_; }

private:
    void boot() {
        session_.reset(new golf::Session(fs_));
        feed_.reset(new golf::GpsFeed(*session_));
        b1_ = golf::Button();
        b2_ = golf::Button();
        power_ = golf::ScreenPower();
    }

    void press(bool button1, bool button2, uint32_t ms) {
        advance(ms, button1, button2);
        advance(200);
    }

    uint32_t nextRandom() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_ >> 8;
    }

    // Ungefaehr normalverteilt, Standardabweichung 1.
    double gauss() {
        double sum = 0.0;
        for (int i = 0; i < 12; i++) sum += static_cast<double>(nextRandom() % 100000) / 100000.0;
        return sum - 6.0;
    }

    void emitBurst() {
        if (!receiver_on) return;
        const uint32_t second = now_ms_ / 1000;
        golf::NmeaFixInfo info;
        info.fix = has_fix;
        info.has_time = has_time;
        info.hour = 8 + static_cast<int>(second / 3600) % 16;
        info.minute = static_cast<int>(second / 60) % 60;
        info.second = static_cast<int>(second % 60);
        info.position = true_position_;
        if (noise_m > 0.0) {
            info.position.lat += gauss() * noise_m / 111180.0;
            info.position.lon += gauss() * noise_m / 75400.0;
        }
        std::string burst = golf::nmeaBurst(info);
        if (corrupt_rate > 0.0) {
            for (char& c : burst) {
                if (static_cast<double>(nextRandom() % 1000000) / 1000000.0 < corrupt_rate) {
                    c = static_cast<char>(c ^ (1 << (nextRandom() % 8)));
                    corrupted_bytes++;
                }
            }
        }
        feed_->feed(burst.data(), burst.size(), now_ms_);
    }

    MemoryFs& fs_;
    uint32_t rng_;
    std::unique_ptr<golf::Session> session_;
    std::unique_ptr<golf::GpsFeed> feed_;
    golf::Button b1_, b2_;
    golf::ScreenPower power_;
    uint32_t now_ms_ = 0;
    golf::LatLon true_position_{47.3769, 8.5417};
    int failed_syncs_ = 0;
};

} // namespace testsupport
