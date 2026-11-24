#include "core/gps_feed.h"

namespace golf {

namespace {
double median3(double a, double b, double c) {
    if (a > b) {
        const double t = a;
        a = b;
        b = t;
    }
    if (b > c) b = c;
    return a > b ? a : b;
}
} // namespace

void GpsFeed::addSample(const LatLon& position, uint32_t now_ms) {
    // Zu alte Positionen entfernen, die neueste ans Ende.
    int kept = 0;
    for (int i = 0; i < sample_count_; i++) {
        if (now_ms - samples_[i].ms <= kSampleMaxAgeMs) samples_[kept++] = samples_[i];
    }
    if (kept == 3) {
        samples_[0] = samples_[1];
        samples_[1] = samples_[2];
        kept = 2;
    }
    samples_[kept] = Sample{position, now_ms};
    sample_count_ = kept + 1;
}

LatLon GpsFeed::filteredPosition() const {
    if (sample_count_ < 3) return samples_[sample_count_ - 1].position; // zu wenig fuer einen Median
    return LatLon{median3(samples_[0].position.lat, samples_[1].position.lat, samples_[2].position.lat),
                  median3(samples_[0].position.lon, samples_[1].position.lon, samples_[2].position.lon)};
}

void GpsFeed::feed(const char* data, size_t length, uint32_t now_ms) {
    for (size_t i = 0; i < length; i++) {
        if (parser_.feed(data[i])) applyFix(now_ms);
    }
}

void GpsFeed::tick(uint32_t now_ms) {
    if (has_last_ && now_ms - last_valid_ms_ >= kTimeoutMs) {
        has_last_ = false;
        rejects_in_a_row_ = 0;
        sample_count_ = 0;
        session_.app().clearPosition();
    }
}

void GpsFeed::applyFix(uint32_t now_ms) {
    const GpsFix& fix = parser_.fix();

    if (fix.has_date && fix.has_time) {
        DateTime now;
        now.valid = true;
        now.year = fix.year;
        now.month = fix.month;
        now.day = fix.day;
        now.hour = fix.hour;
        now.minute = fix.minute;
        session_.setDateTime(now);
    }

    if (!fix.valid) {
        has_last_ = false;
        rejects_in_a_row_ = 0;
        sample_count_ = 0;
        session_.app().clearPosition();
        return;
    }

    if (has_last_) {
        // Verglichen wird mit der angezeigten (gefilterten) Position. Der
        // Empfaenger misst einmal pro Sekunde; GGA und RMC derselben Sekunde
        // folgen direkt aufeinander und zaehlen deshalb als eine Sekunde.
        double seconds = (now_ms - last_valid_ms_) / 1000.0;
        if (seconds < 1.0) seconds = 1.0;
        const double speed = distanceMeters(last_position_, fix.position) / seconds;
        if (speed > kMaxSpeedMps && rejects_in_a_row_ < kMaxRejects) {
            rejects_in_a_row_++;
            rejected_jumps_++;
            return; // alte Position bleibt gueltig
        }
        // Nach mehreren "Spruengen" in Folge gilt der neue Ort; der alte zaehlt nicht mehr mit.
        if (speed > kMaxSpeedMps) sample_count_ = 0;
    }

    rejects_in_a_row_ = 0;
    has_last_ = true;
    last_valid_ms_ = now_ms;
    addSample(fix.position, now_ms);
    last_position_ = filteredPosition();
    session_.app().setPosition(last_position_);
}

} // namespace golf
