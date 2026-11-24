// Dauerhafte Kennungen fuer Runden, Plaetze und das Geraet: 16 Hex-Zeichen
// (64 Bit). Damit erkennt die App, was sie schon hat, auch wenn Nummern auf
// dem Geraet neu vergeben werden.
#pragma once

#include <cstdint>
#include <string>

namespace golf {

constexpr int kIdLength = 16;
using Id = char[kIdLength + 1];

// Liefert Zufallszahlen. Auf dem Geraet der Hardware-Zufallsgenerator.
using RandomFn = uint32_t (*)();

// Erzeugt eine neue Kennung. Ohne Zufallsquelle (nullptr) entsteht sie aus
// `seed` und einem Zaehler; das genuegt fuer Tests, nicht fuer das Geraet.
void makeId(Id out, RandomFn random, uint32_t seed = 0);

// Kennung, die allein vom Inhalt abhaengt. Fuer alte Dateien ohne Kennung:
// Sie bekommen so bei jedem Laden dieselbe.
void idFromContent(Id out, const std::string& content);

// Genau 16 Zeichen aus 0-9 und a-f.
bool isValidId(const char* text);

} // namespace golf
