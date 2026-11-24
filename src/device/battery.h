// Akkustand ueber den Stromversorgungs-Chip AXP2101.
#pragma once

#include "core/app.h"

namespace device {

// Startet die Verbindung zum Chip. false, wenn er nicht antwortet.
bool batteryBegin();

// Liest den aktuellen Stand. Ohne Chip oder ohne Akku ist `known` false.
golf::BatteryInfo batteryRead();

} // namespace device
