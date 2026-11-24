#include "core/power.h"

namespace golf {

void ScreenPower::wake(uint32_t now_ms) {
    state_ = ScreenState::Active;
    last_input_ms_ = now_ms;
    started_ = true;
}

bool ScreenPower::onInput(uint32_t now_ms) {
    const bool was_off = state_ == ScreenState::Off;
    wake(now_ms);
    return !was_off;
}

bool ScreenPower::onTouch(bool pressed, uint32_t now_ms) {
    if (!pressed) {
        swallowing_touch_ = false;
        touch_was_pressed_ = false;
        return false;
    }
    if (!touch_was_pressed_ && state_ == ScreenState::Off) swallowing_touch_ = true; // diese Beruehrung weckt nur
    touch_was_pressed_ = true;
    wake(now_ms);
    return !swallowing_touch_;
}

void ScreenPower::tick(uint32_t now_ms, bool keep_on) {
    if (!started_) { // die Zeit laeuft ab dem ersten Aufruf
        started_ = true;
        last_input_ms_ = now_ms;
    }
    // Einmal aus bleibt aus bis zur naechsten Eingabe, auch wenn der Zeitzaehler ueberlaeuft.
    if (state_ == ScreenState::Off) {
        if (keep_on) state_ = ScreenState::Dim;
        return;
    }
    const uint32_t idle = now_ms - last_input_ms_;
    if (idle >= off_after_ms_ && !keep_on) {
        state_ = ScreenState::Off;
    } else if (idle >= dim_after_ms_) {
        state_ = ScreenState::Dim;
        // Damit "gedimmt" bei sehr langer Wartezeit mit keep_on nicht durch einen
        // Ueberlauf wieder hell wird, wandert der Bezugspunkt mit.
        if (idle > off_after_ms_ * 2) last_input_ms_ = now_ms - off_after_ms_;
    } else {
        state_ = ScreenState::Active;
    }
}

uint8_t ScreenPower::brightness() const {
    switch (state_) {
        case ScreenState::Active: return kBrightnessActive;
        case ScreenState::Dim: return kBrightnessDim;
        case ScreenState::Off: break;
    }
    return 0;
}

void dispatchButton(App& app, ScreenPower& power, int button, ButtonEvent event, uint32_t now_ms) {
    if (event == ButtonEvent::None) return;
    if (power.onInput(now_ms)) dispatchButton(app, button, event);
}

} // namespace golf
