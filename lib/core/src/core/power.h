// Stromsparen am Display und Akkustand.
#pragma once

#include <cstdint>

#include "core/app.h"
#include "core/button.h"
#include "core/settings.h"

namespace golf {

// Helligkeit des Displays abhaengig davon, wie lange niemand etwas bedient hat.
//
//   Hell      nach jeder Eingabe
//   Gedimmt   nach 20 Sekunden ohne Eingabe; weiterhin lesbar, Eingaben wirken normal
//   Aus       nach 30 Minuten ohne Eingabe; die erste Eingabe weckt nur das Display
//             und loest sonst nichts aus (kein versehentlicher Schlag)
//
// Die 30 Minuten sind bewusst lang: Zwischen zwei Schlaegen liegen auf dem
// Platz oft mehr als fuenf Minuten (Weg zum naechsten Abschlag, Warten). In
// dieser Zeit darf das Display nicht ausgehen, sonst wuerde der naechste
// Schlag nur das Display wecken und nicht gezaehlt. "Aus" ist fuer das
// vergessene Geraet nach der Runde gedacht.
//
// GPS und Zaehlung laufen in jedem Zustand weiter.
enum class ScreenState : uint8_t { Active, Dim, Off };

class ScreenPower {
public:
    static constexpr uint32_t kDimAfterMs = 20000;
    static constexpr uint32_t kOffAfterMs = 30 * 60 * 1000;
    static constexpr uint8_t kBrightnessActive = 200;
    static constexpr uint8_t kBrightnessDim = 40;

    explicit ScreenPower(uint32_t dim_after_ms = kDimAfterMs, uint32_t off_after_ms = kOffAfterMs)
        : dim_after_ms_(dim_after_ms), off_after_ms_(off_after_ms) {}

    // Uebernimmt Helligkeit und Dimmzeit aus den Einstellungen. Gedimmt ist
    // das Display hoechstens halb so hell wie hell.
    void configure(const DeviceSettings& settings);

    // Bei jedem Knopf-Ereignis aufrufen. Liefert true, wenn die Eingabe an die
    // App weitergehen soll, und false, wenn sie nur das Display geweckt hat.
    bool onInput(uint32_t now_ms);

    // Bei jeder Abfrage des Touchscreens aufrufen. Liefert, ob die Beruehrung
    // an die Oberflaeche weitergehen soll. Eine Beruehrung, die das Display
    // weckt, wird bis zum Loslassen ganz verschluckt.
    bool onTouch(bool pressed, uint32_t now_ms);

    // Regelmaessig aufrufen. Mit `keep_on` geht das Display nie ganz aus
    // (z. B. solange "Mit Handy verbinden" offen ist).
    void tick(uint32_t now_ms, bool keep_on);

    ScreenState state() const { return state_; }
    uint8_t brightness() const;

private:
    void wake(uint32_t now_ms);

    uint32_t dim_after_ms_;
    uint32_t off_after_ms_;
    uint8_t active_brightness_ = kBrightnessActive;
    ScreenState state_ = ScreenState::Active;
    bool started_ = false;
    uint32_t last_input_ms_ = 0;
    bool swallowing_touch_ = false;
    bool touch_was_pressed_ = false;
};

// Leitet ein Knopf-Ereignis an die App weiter, ausser es weckt nur das Display.
void dispatchButton(App& app, ScreenPower& power, int button, ButtonEvent event, uint32_t now_ms);

// Dasselbe fuer einen Knopf am Geraet: beruecksichtigt, ob die Knoepfe
// getauscht sind.
void dispatchPhysicalButton(App& app, ScreenPower& power, const DeviceSettings& settings, int physical_button,
                            ButtonEvent event, uint32_t now_ms);

} // namespace golf
