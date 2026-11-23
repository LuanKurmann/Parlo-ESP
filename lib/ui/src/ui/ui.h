// Oberflaeche der Golf-App. Laeuft unveraendert auf dem Geraet und im Simulator.
#pragma once

namespace golf {
namespace ui {

// Baut den Startbildschirm auf dem aktiven LVGL-Display auf.
// Voraussetzung: lv_init() wurde aufgerufen und ein Display existiert.
void init();

} // namespace ui
} // namespace golf
