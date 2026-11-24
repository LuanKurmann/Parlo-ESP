#include "demo_track.h"

#include <cstdint>

#include "core/demo_course.h"
#include "core/nmea_writer.h"

namespace sim {

namespace {

struct Recorder {
    std::string text;
    int second = 0;
    golf::LatLon position{0.0, 0.0};
    uint32_t rng = 20261124;

    double noise() { // etwa normalverteilt, Standardabweichung 1 m
        double sum = 0.0;
        for (int i = 0; i < 12; i++) {
            rng = rng * 1664525u + 1013904223u;
            sum += static_cast<double>((rng >> 8) % 100000) / 100000.0;
        }
        return sum - 6.0;
    }

    void record(bool fix) {
        golf::NmeaFixInfo info;
        info.fix = fix;
        info.hour = 8 + second / 3600;
        info.minute = (second / 60) % 60;
        info.second = second % 60;
        info.position = golf::LatLon{position.lat + noise() / 111180.0, position.lon + noise() / 75400.0};
        text += golf::nmeaBurst(info);
        second++;
    }

    void stand(int seconds, bool fix = true) {
        for (int i = 0; i < seconds; i++) record(fix);
    }

    void walkTo(const golf::LatLon& target, double speed = 1.4) {
        for (;;) {
            const double remaining = golf::distanceMeters(position, target);
            if (remaining <= speed) break;
            const double f = speed / remaining;
            position.lat += (target.lat - position.lat) * f;
            position.lon += (target.lon - position.lon) * f;
            record(true);
        }
        position = target;
        record(true);
    }
};

} // namespace

std::string demoTrack(int holes) {
    const golf::Course course = golf::demoCourse();
    Recorder r;
    r.position = course.holes[0].tee;
    r.stand(8, false); // Kaltstart ohne Position
    for (int h = 0; h < holes && h < golf::kMaxHoles; h++) {
        const golf::Hole& hole = course.holes[h];
        const double length = golf::distanceMeters(hole.tee, hole.green_center);
        r.walkTo(hole.tee, 2.5);
        r.stand(20); // Abschlag
        if (hole.par >= 4) {
            r.walkTo(golf::positionOnHole(hole, length * 0.6, h % 2 ? 12.0 : -8.0));
            r.stand(15);
        }
        if (hole.par >= 5) {
            r.walkTo(golf::positionOnHole(hole, length * 0.87, 5.0));
            r.stand(15);
        }
        r.walkTo(golf::positionOnHole(hole, length - 7.0, 1.0)); // erster Putt
        r.stand(15);
        r.walkTo(golf::positionOnHole(hole, length - 1.0));
        r.stand(15);
    }
    return r.text;
}

} // namespace sim
