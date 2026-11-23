#include "core/round.h"

namespace golf {

Round::Round(int hole_count) : hole_count_(hole_count) {
    if (hole_count_ < 1) hole_count_ = 1;
    if (hole_count_ > kMaxHoles) hole_count_ = kMaxHoles;
}

bool Round::pushShot(int hole, const Shot& shot) {
    if (!validHole(hole)) return false;
    HoleData& h = holes_[hole];
    if (h.count >= kMaxShotsPerHole) return false;
    h.shots[h.count++] = shot;
    h.finished = false;
    return true;
}

bool Round::addShot(int hole, const LatLon& position) {
    Shot s;
    s.has_position = true;
    s.position = position;
    return pushShot(hole, s);
}

bool Round::addShotWithoutPosition(int hole) { return pushShot(hole, Shot{}); }

bool Round::setClub(int hole, int shot, Club club) {
    if (!validHole(hole) || shot < 0 || shot >= holes_[hole].count) return false;
    holes_[hole].shots[shot].club = club;
    return true;
}

bool Round::removeLastShot(int hole) {
    if (!validHole(hole) || holes_[hole].count == 0) return false;
    HoleData& h = holes_[hole];
    h.count--;
    h.shots[h.count] = Shot{};
    if (h.count == 0) h.finished = false;
    return true;
}

bool Round::finishHole(int hole) {
    if (!validHole(hole) || holes_[hole].count == 0) return false;
    holes_[hole].finished = true;
    return true;
}

bool Round::isFinished(int hole) const { return validHole(hole) && holes_[hole].finished; }

int Round::shotCount(int hole) const { return validHole(hole) ? holes_[hole].count : 0; }

int Round::putts(int hole) const {
    if (!validHole(hole)) return 0;
    int n = 0;
    for (int i = 0; i < holes_[hole].count; i++) {
        if (holes_[hole].shots[i].club == Club::Putter) n++;
    }
    return n;
}

const Shot* Round::shot(int hole, int index) const {
    if (!validHole(hole) || index < 0 || index >= holes_[hole].count) return nullptr;
    return &holes_[hole].shots[index];
}

ShotDistance shotDistance(const Round& round, const Course& course, int hole, int shot, const LatLon* player) {
    ShotDistance result;
    const Shot* s = round.shot(hole, shot);
    if (s == nullptr || !s->has_position || s->club == Club::Putter) return result;

    const LatLon* end = nullptr;
    const Shot* next = round.shot(hole, shot + 1);
    if (next != nullptr) {
        if (next->has_position) end = &next->position;
    } else if (round.isFinished(hole)) {
        if (course.holes[hole].has_green) end = &course.holes[hole].green_center;
    } else {
        end = player;
    }
    if (end == nullptr) return result;

    result.available = true;
    result.meters = distanceMeters(s->position, *end);
    return result;
}

RoundTotals roundTotals(const Round& round, const Course& course) {
    RoundTotals t;
    for (int h = 0; h < round.holeCount(); h++) {
        if (!round.isFinished(h)) continue;
        const int strokes = round.shotCount(h);
        t.holes_played++;
        t.strokes += strokes;
        t.putts += round.putts(h);
        if (course.holes[h].par > 0) {
            t.holes_with_par++;
            t.to_par += strokes - course.holes[h].par;
        }
        for (int i = 0; i < strokes; i++) {
            const ShotDistance d = shotDistance(round, course, h, i, nullptr);
            if (d.available && d.meters > t.longest_shot_m) t.longest_shot_m = d.meters;
        }
    }
    return t;
}

ScoreName scoreName(int strokes, int par) {
    if (strokes <= 0 || par <= 0) return ScoreName::Unknown;
    if (strokes == 1) return ScoreName::HoleInOne;
    const int diff = strokes - par;
    if (diff <= -3) return ScoreName::Albatross;
    if (diff == -2) return ScoreName::Eagle;
    if (diff == -1) return ScoreName::Birdie;
    if (diff == 0) return ScoreName::Par;
    if (diff == 1) return ScoreName::Bogey;
    if (diff == 2) return ScoreName::DoubleBogey;
    return ScoreName::Worse;
}

} // namespace golf
