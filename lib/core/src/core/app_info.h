// Grunddaten der App, die Kern, Oberflaeche und Tests gemeinsam nutzen.
#pragma once

namespace golf {

// Sichtbare Aufloesung des runden AMOLED in Pixeln.
constexpr int kDisplaySize = 466;

// Versionsnummer der Firmware, z. B. "0.1.0".
const char* version();

} // namespace golf
