// Liest die NMEA-Saetze des GPS-Empfaengers und haelt den aktuellen Stand.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/geo.h"

namespace golf {

// Letzter bekannter Stand des GPS.
struct GpsFix {
    bool valid = false;       // true, solange der Empfaenger eine gueltige Position meldet
    LatLon position{0.0, 0.0}; // nur aussagekraeftig, wenn valid
    int satellites = 0;       // genutzte Satelliten (aus GGA)
    float hdop = 99.9f;       // horizontale Genauigkeit, kleiner ist besser (aus GGA)
    float altitude_m = 0.0f;  // Hoehe ueber Meer (aus GGA)
    float speed_kmh = 0.0f;   // Geschwindigkeit (aus RMC)

    bool has_time = false;    // UTC-Uhrzeit
    int hour = 0, minute = 0, second = 0;
    bool has_date = false;    // UTC-Datum (aus RMC)
    int year = 0, month = 0, day = 0;
};

// Verarbeitet den Zeichenstrom des Empfaengers Zeichen fuer Zeichen.
//
// Ausgewertet werden GGA und RMC, unabhaengig vom Satellitensystem (GP, GN,
// GL, GA, GB ...). Saetze ohne oder mit falscher Pruefsumme werden verworfen.
class NmeaParser {
public:
    // Nimmt ein Zeichen entgegen. Liefert true, wenn damit ein Satz
    // abgeschlossen wurde, der den Stand in fix() veraendert hat.
    bool feed(char c);

    // Nimmt mehrere Zeichen entgegen. Liefert true, wenn mindestens ein Satz
    // den Stand veraendert hat.
    bool feed(const char* data, size_t length);

    const GpsFix& fix() const { return fix_; }

    // Zaehler fuer die Diagnose.
    uint32_t acceptedSentences() const { return accepted_; }
    uint32_t rejectedSentences() const { return rejected_; }

private:
    static constexpr size_t kMaxLength = 120;

    bool processSentence();
    bool handleGga(char** fields, int count);
    bool handleRmc(char** fields, int count);

    char buffer_[kMaxLength + 1] = {};
    size_t length_ = 0;
    bool in_sentence_ = false;
    GpsFix fix_;
    uint32_t accepted_ = 0;
    uint32_t rejected_ = 0;
};

} // namespace golf
