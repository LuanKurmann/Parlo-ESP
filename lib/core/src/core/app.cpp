#include "core/app.h"

namespace golf {

namespace {
int clampHoleCount(int n) { return n < 1 ? 1 : (n > kMaxHoles ? kMaxHoles : n); }
} // namespace

App::App(const Course& course, const Bag& bag)
    : course_(course), bag_(bag), round_(clampHoleCount(course.hole_count)), previous_round_(round_.holeCount()) {
    course_.hole_count = round_.holeCount();
}

bool App::restoreRound(const Round& round, int hole) {
    if (round.holeCount() != round_.holeCount() || hole < 0 || hole >= round.holeCount()) return false;
    round_ = round;
    hole_ = hole;
    view_ = View::Distance;
    overlay_ = Overlay::None;
    // Wurde mitten in der Schlaegerwahl ausgeschaltet, geht es dort weiter.
    const int count = round_.shotCount(hole_);
    if (count > 0 && round_.shot(hole_, count - 1)->club == Club::None) {
        suggested_ = suggestClub(bag_, kDistanceUnknown, count == 1, course_.holes[hole_].par);
        overlay_ = Overlay::ClubPicker;
    }
    return true;
}

void App::setPosition(const LatLon& position) {
    position_ = position;
    has_position_ = true;
}

void App::clearPosition() { has_position_ = false; }

Message App::takeMessage() {
    const Message m = message_;
    message_ = Message::None;
    return m;
}

GreenDistances App::distances() const {
    if (!has_position_) return GreenDistances();
    return greenDistances(course_.holes[hole_], position_);
}

ShotDistance App::lastShotDistance() const {
    const int count = round_.shotCount(hole_);
    if (count == 0) return ShotDistance();
    return shotDistance(round_, course_, hole_, count - 1, has_position_ ? &position_ : nullptr);
}

// ---------- Knoepfe ----------

void App::button1Short() {
    switch (overlay_) {
        case Overlay::None: addShot(); break;
        case Overlay::ClubPicker: confirmClub(suggested_); break;
        case Overlay::Menu: runMenuItem(menu_); break;
        case Overlay::HoleDone: nextHole(); break;
        case Overlay::RoundSummary: break; // neue Runde nur mit langem Druck
    }
}

void App::button1Long() {
    switch (overlay_) {
        case Overlay::None:
        case Overlay::ClubPicker: undo(); break;
        case Overlay::RoundSummary: newRound(); break;
        case Overlay::Menu:
        case Overlay::HoleDone: break;
    }
}

void App::button2Short() {
    switch (overlay_) {
        case Overlay::None: finishHole(); break;
        case Overlay::ClubPicker: suggested_ = nextClub(suggested_); break;
        case Overlay::Menu: menu_ = static_cast<MenuItem>((static_cast<int>(menu_) + 1) % kMenuItemCount); break;
        case Overlay::HoleDone: nextHole(); break;
        case Overlay::RoundSummary: overlay_ = Overlay::None; break;
    }
}

void App::button2Long() {
    if (overlay_ == Overlay::None) {
        overlay_ = Overlay::Menu;
        menu_ = MenuItem::MarkGreen;
    } else if (overlay_ == Overlay::Menu) {
        overlay_ = Overlay::None;
    }
}

// ---------- Touch ----------

void App::swipe(int direction) {
    if (overlay_ != Overlay::None || direction == 0) return;
    const int step = direction > 0 ? 1 : kViewCount - 1;
    view_ = static_cast<View>((static_cast<int>(view_) + step) % kViewCount);
}

void App::selectClub(Club club) {
    if (overlay_ != Overlay::ClubPicker || static_cast<int>(club) >= kClubCount) return;
    confirmClub(club);
}

void App::selectMenuItem(MenuItem item) {
    if (overlay_ != Overlay::Menu || static_cast<int>(item) >= kMenuItemCount) return;
    runMenuItem(item);
}

void App::tap() {
    if (overlay_ == Overlay::HoleDone) nextHole();
}

// ---------- Ablauf ----------

void App::addShot() {
    const bool added = has_position_ ? round_.addShot(hole_, position_) : round_.addShotWithoutPosition(hole_);
    if (!added) {
        message_ = Message::HoleFull;
        return;
    }
    const GreenDistances d = distances();
    suggested_ = suggestClub(bag_, d.available ? d.center : kDistanceUnknown, round_.shotCount(hole_) == 1,
                             course_.holes[hole_].par);
    overlay_ = Overlay::ClubPicker;
}

void App::confirmClub(Club club) {
    round_.setClub(hole_, round_.shotCount(hole_) - 1, club);
    overlay_ = Overlay::None;
}

void App::undo() {
    overlay_ = Overlay::None;
    message_ = round_.removeLastShot(hole_) ? Message::ShotRemoved : Message::NoShotToRemove;
}

void App::finishHole() {
    if (!round_.finishHole(hole_)) {
        message_ = Message::NoShotYet;
        return;
    }
    overlay_ = Overlay::HoleDone;
}

void App::nextHole() {
    if (hole_ >= round_.holeCount() - 1) {
        overlay_ = Overlay::RoundSummary;
        return;
    }
    hole_++;
    view_ = View::Distance;
    overlay_ = Overlay::None;
}

void App::runMenuItem(MenuItem item) {
    overlay_ = Overlay::None;
    switch (item) {
        case MenuItem::MarkGreen:
            if (has_position_) {
                markGreen(course_.holes[hole_], position_);
                message_ = Message::GreenMarked;
            } else {
                message_ = Message::NoGpsForGreen;
            }
            break;
        case MenuItem::PreviousHole:
            if (hole_ > 0) {
                hole_--;
            } else {
                message_ = Message::AlreadyFirstHole;
            }
            break;
        case MenuItem::EndRound: overlay_ = Overlay::RoundSummary; break;
        case MenuItem::Close: break;
    }
}

void App::newRound() {
    previous_round_ = round_;
    round_number_++;
    round_ = Round(course_.hole_count);
    hole_ = 0;
    view_ = View::Distance;
    overlay_ = Overlay::None;
    message_ = Message::NewRoundStarted;
}

void dispatchButton(App& app, int button, ButtonEvent event) {
    if (event == ButtonEvent::None) return;
    const bool is_long = event == ButtonEvent::LongPress;
    if (button == 1) {
        is_long ? app.button1Long() : app.button1Short();
    } else if (button == 2) {
        is_long ? app.button2Long() : app.button2Short();
    }
}

} // namespace golf
