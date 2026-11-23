#include "core/button.h"

namespace golf {

ButtonEvent Button::update(bool pressed, uint32_t now_ms) {
    if (pressed != raw_) {
        raw_ = pressed;
        raw_since_ = now_ms;
    }

    // Der Rohzustand gilt erst, wenn er debounce_ms lang unveraendert anliegt.
    if (raw_ != stable_ && now_ms - raw_since_ >= debounce_ms_) {
        stable_ = raw_;
        if (stable_) {
            pressed_at_ = raw_since_;
            long_fired_ = false;
        } else if (!long_fired_) {
            return ButtonEvent::ShortPress;
        }
    }

    // Nur solange der Knopf auch roh gedrueckt ist: Wer kurz vor der Schwelle
    // loslaesst, bekommt ein Kurz, auch wenn das Loslassen noch entprellt wird.
    if (stable_ && raw_ && !long_fired_ && now_ms - pressed_at_ >= long_ms_) {
        long_fired_ = true;
        return ButtonEvent::LongPress;
    }
    return ButtonEvent::None;
}

} // namespace golf
