// Spielt eine aufgezeichnete NMEA-Datei sekundenweise ab.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace golf {

class NmeaLog {
public:
    // `text` ist der Inhalt der Datei. Zeilen, die nicht mit "$" beginnen,
    // werden uebersprungen.
    explicit NmeaLog(const std::string& text);

    // Anzahl Sekunden (Messzeitpunkte) in der Aufzeichnung.
    size_t burstCount() const { return bursts_.size(); }

    // Liefert die Saetze des naechsten Messzeitpunkts. false am Ende.
    bool next(std::string& burst);

    size_t position() const { return next_; }
    void rewind() { next_ = 0; }

private:
    std::vector<std::string> bursts_;
    size_t next_ = 0;
};

} // namespace golf
