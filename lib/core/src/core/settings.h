// Einstellungen des Geraets, die nicht zum Bag gehoeren.
#pragma once

namespace golf {

struct DeviceSettings {
    int brightness = 200;      // Helligkeit des Displays im hellen Zustand
    int dim_after_s = 20;      // Sekunden ohne Eingabe, bis das Display dimmt
    bool swap_buttons = false; // Knopf 1 und Knopf 2 tauschen (je nach Tragweise)
};

constexpr int kMinBrightness = 20; // nie so dunkel, dass man nichts mehr sieht
constexpr int kMaxBrightness = 255;
constexpr int kMinDimAfterS = 5;
constexpr int kMaxDimAfterS = 600;

// Welcher Knopf der App gemeint ist, wenn der Knopf `physical` (1 oder 2)
// gedrueckt wird.
inline int mappedButton(int physical, const DeviceSettings& settings) {
    if (!settings.swap_buttons || (physical != 1 && physical != 2)) return physical;
    return 3 - physical;
}

} // namespace golf
