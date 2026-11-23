// Speichert den aktuellen Bildschirminhalt als BMP-Datei.
#pragma once

#include <string>

namespace sim {

// Pixel ausserhalb des runden Displays werden in der Gehaeusefarbe gespeichert,
// so wie man das Geraet sieht. Liefert false bei einem Fehler.
bool saveScreenshot(const std::string& path);

} // namespace sim
