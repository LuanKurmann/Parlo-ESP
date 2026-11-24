#include "core/session.h"

#include <cstring>
#include <memory>

#include "core/crc32.h"

namespace golf {

namespace {

bool hasShots(const Round& round) {
    for (int h = 0; h < round.holeCount(); h++) {
        if (round.shotCount(h) > 0) return true;
    }
    return false;
}

uint32_t crcOf(const std::string& s) { return crc32(s.data(), s.size()); }

} // namespace

Course Session::loadCourseOrEmpty(Store& store) {
    Course course;
    if (!store.loadCourse(course)) course = Course();
    return course;
}

std::string Session::currentRoundJson() const {
    return roundToJson(app_.round(), app_.currentHole(), started_, app_.course().name, &app_.course());
}

bool Session::replaceCourse(const Course& course, bool* archived) {
    if (archived != nullptr) *archived = false;
    if (hasShots(app_.round())) {
        if (store_.archiveRound(app_.round(), started_, app_.course().name, &app_.course()) == 0) return false;
        if (archived != nullptr) *archived = true;
    }
    app_.setCourse(course);
    started_ = DateTime();
    pending_ = true;
    return sync();
}

bool Session::setBag(const Bag& bag) {
    if (!store_.saveBag(bag)) return false;
    app_.setBag(bag);
    return true;
}

Session::Session(FileSystem& fs) : store_(fs), app_(loadCourseOrEmpty(store_)) {
    Bag bag;
    if (store_.loadBag(bag)) app_.setBag(bag);

    // Was jetzt im Speicher liegt, muss nicht gleich wieder geschrieben werden.
    Course stored;
    if (store_.loadCourse(stored)) {
        course_saved_ = true;
        course_crc_ = crcOf(courseToJson(app_.course()));
    }

    std::unique_ptr<RoundState> state(new RoundState());
    if (store_.loadCurrentRound(*state) && app_.restoreRound(state->round, state->hole)) {
        started_ = state->started;
        round_saved_ = true;
        round_crc_ = crcOf(currentRoundJson());
    }
}

bool Session::sync() {
    if (!pending_ && last_sync_ok_ && app_.inputCount() == seen_input_count_) return true;
    pending_ = false;
    seen_input_count_ = app_.inputCount();

    bool ok = true;

    if (app_.roundNumber() != seen_round_number_) {
        seen_round_number_ = app_.roundNumber();
        previous_started_ = started_;
        started_ = DateTime();
        archive_pending_ = hasShots(app_.previousRound());
    }
    if (archive_pending_) {
        if (store_.archiveRound(app_.previousRound(), previous_started_, app_.course().name, &app_.course()) > 0) {
            archive_pending_ = false;
        } else {
            ok = false;
        }
    }

    if (!started_.valid && now_.valid && hasShots(app_.round())) started_ = now_;

    const std::string round_json = currentRoundJson();
    const uint32_t round_crc = crcOf(round_json);
    if (!round_saved_ || round_crc != round_crc_) {
        if (store_.saveCurrentRoundJson(round_json)) {
            round_saved_ = true;
            round_crc_ = round_crc;
        } else {
            ok = false;
        }
    }

    const std::string course_json = courseToJson(app_.course());
    const uint32_t course_crc = crcOf(course_json);
    if (!course_saved_ || course_crc != course_crc_) {
        if (store_.saveCourseJson(course_json)) {
            course_saved_ = true;
            course_crc_ = course_crc;
        } else {
            ok = false;
        }
    }

    last_sync_ok_ = ok;
    return ok;
}

} // namespace golf
