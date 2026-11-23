// Oberflaeche der Golf-App. Laeuft unveraendert auf dem Geraet und im Simulator.
#pragma once

#include "core/app.h"

namespace golf {
namespace ui {

// Baut die Oberflaeche auf dem aktiven LVGL-Display auf.
// Voraussetzung: lv_init() wurde aufgerufen und ein Display existiert.
// Die App muss so lange leben wie die Oberflaeche.
void init(App& app);

// Uebernimmt den aktuellen Zustand der App in die Anzeige. Nach jeder Eingabe
// und regelmaessig (neue GPS-Position) aufrufen; zeichnet nur, was sich
// geaendert hat.
void refresh();

} // namespace ui
} // namespace golf
