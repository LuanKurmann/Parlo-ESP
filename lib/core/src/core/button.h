// Erkennt kurzes und langes Druecken eines Knopfs.
#pragma once

#include <cstdint>

namespace golf {

enum class ButtonEvent : uint8_t { None, ShortPress, LongPress };

// Wird regelmaessig (alle paar Millisekunden) mit dem Rohzustand des Knopfs
// und der aktuellen Zeit aufgerufen.
//
//   Kurz: der Knopf wird vor Ablauf von long_ms losgelassen. Das Ereignis
//         kommt beim Loslassen.
//   Lang: der Knopf wird long_ms gehalten. Das Ereignis kommt sofort nach
//         Ablauf der Zeit, noch waehrend der Knopf gedrueckt ist, und nur einmal.
//
// Zustandswechsel, die kuerzer als debounce_ms dauern (Prellen, Stoersignale),
// werden ignoriert. Der Zeitzaehler darf ueberlaufen.
class Button {
public:
    explicit Button(uint32_t long_ms = 550, uint32_t debounce_ms = 30)
        : long_ms_(long_ms), debounce_ms_(debounce_ms) {}

    ButtonEvent update(bool pressed, uint32_t now_ms);

    bool isDown() const { return stable_; }

private:
    uint32_t long_ms_;
    uint32_t debounce_ms_;
    bool raw_ = false;          // letzter Rohzustand
    uint32_t raw_since_ = 0;    // seit wann der Rohzustand anliegt
    bool stable_ = false;       // entprellter Zustand
    uint32_t pressed_at_ = 0;   // Beginn des entprellten Drucks
    bool long_fired_ = false;
};

} // namespace golf
