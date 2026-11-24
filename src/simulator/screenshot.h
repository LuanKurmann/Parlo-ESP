// Speichert den aktuellen Bildschirminhalt als BMP-Datei.
#pragma once

#include <string>

namespace sim {

// Mit `round_mask` werden Pixel ausserhalb des runden Displays in der
// Gehaeusefarbe gespeichert, so wie man das Geraet sieht. Ohne Maske bleibt
// das ganze Quadrat erhalten; damit laesst sich pruefen, ob etwas ueber den
// runden Rand hinaus gezeichnet wurde. Liefert false bei einem Fehler.
bool saveScreenshot(const std::string& path, bool round_mask = true);

} // namespace sim
