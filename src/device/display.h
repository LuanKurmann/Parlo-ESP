// AMOLED und Touch des Geraets als LVGL-Display und LVGL-Eingabegeraet.
#pragma once

#include <cstdint>

namespace device {

// Startet das Display und meldet es bei LVGL an. Voraussetzung: lv_init().
// Liefert false, wenn das Display nicht antwortet.
bool displayBegin();

// Helligkeit 0 bis 255.
void displaySetBrightness(uint8_t value);

// Startet den Touch-Controller und meldet ihn bei LVGL an. Voraussetzung:
// Wire laeuft und displayBegin() war erfolgreich.
bool touchBegin();

} // namespace device
