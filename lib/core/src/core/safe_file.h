// Dateien so schreiben und lesen, dass ein Stromausfall mitten im Speichern
// nie zu verlorenen oder halb geschriebenen Daten fuehrt.
#pragma once

#include <string>

#include "core/fs.h"

namespace golf {

// Groesster Inhalt, der gelesen wird. Schuetzt vor unsinnig grossen Dateien.
constexpr size_t kMaxPayloadBytes = 256 * 1024;

// Schreibt den Inhalt mit Pruefsumme. Ablauf:
//   1. neue Fassung vollstaendig nach "<path>.new" schreiben
//   2. bisherige Datei loeschen
//   3. "<path>.new" nach "<path>" umbenennen
// Faellt in irgendeinem Schritt der Strom aus, liefert loadSafely danach
// entweder die alte oder die neue Fassung vollstaendig: Bis Schritt 1 fertig
// ist, gilt die alte Datei, danach die neue.
bool saveSafely(FileSystem& fs, const std::string& path, const std::string& payload);

// Liest die neueste unbeschaedigte Fassung. false, wenn es keine gibt.
// Bringt dabei ein unterbrochenes Speichern zu Ende, damit ein spaeterer
// Stromausfall nicht auf einen noch aelteren Stand zurueckfaellt.
bool loadSafely(FileSystem& fs, const std::string& path, std::string& payload);

// Verpackt bzw. prueft einen Inhalt mit Kopfzeile "GOLF1 <crc32> <laenge>".
std::string wrapPayload(const std::string& payload);
bool unwrapPayload(const std::string& file_content, std::string& payload);

} // namespace golf
