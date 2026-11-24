// Verbindet den Zeichenstrom des GPS-Empfaengers mit der App: Position
// setzen, Empfangsverlust erkennen, falsche Positionen aussortieren.
//
// Die Pruefsumme von NMEA ist schwach: Bei gestoerter Leitung besteht sie ab
// und zu auch ein verfaelschter Satz. Dagegen wirken zwei Stufen:
//   1. Spruenge, die schneller als kMaxSpeedMps waeren, werden verworfen.
//   2. Angezeigt wird der Median der letzten drei Positionen. Ein einzelner
//      Ausreisser, egal wie gross, kommt so nie zur Anzeige. Weil der
//      Empfaenger jede Position doppelt sendet (GGA und RMC), entsteht dabei
//      keine spuerbare Verzoegerung.
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/nmea.h"
#include "core/session.h"

namespace golf {

class GpsFeed {
public:
    // Ohne gueltige Position seit kTimeoutMs gilt der Empfang als verloren
    // (Empfaenger ausgefallen oder Kabel ab), auch wenn er das nicht meldet.
    static constexpr uint32_t kTimeoutMs = 6000;
    // Schneller kann sich ein Golfer nicht bewegen (rund 108 km/h). Was darueber
    // liegt, ist ein Uebertragungsfehler, der die Pruefsumme zufaellig bestanden hat.
    static constexpr double kMaxSpeedMps = 30.0;
    // Nach so vielen "unplausiblen" Positionen in Folge wird die neue doch
    // uebernommen: Dann war die alte falsch oder das Geraet wurde verlegt.
    static constexpr int kMaxRejects = 3;
    // Aeltere Positionen zaehlen fuer den Median nicht mehr mit.
    static constexpr uint32_t kSampleMaxAgeMs = 3000;

    explicit GpsFeed(Session& session) : session_(session) {}

    // Zeichen vom Empfaenger, so wie sie von der seriellen Schnittstelle kommen.
    void feed(const char* data, size_t length, uint32_t now_ms);

    // Regelmaessig aufrufen, damit ein Empfangsverlust bemerkt wird.
    void tick(uint32_t now_ms);

    const GpsFix& fix() const { return parser_.fix(); }
    uint32_t rejectedJumps() const { return rejected_jumps_; }

private:
    void applyFix(uint32_t now_ms);

    Session& session_;
    NmeaParser parser_;
    struct Sample {
        LatLon position;
        uint32_t ms;
    };

    void addSample(const LatLon& position, uint32_t now_ms);
    LatLon filteredPosition() const;

    bool has_last_ = false;
    LatLon last_position_{0.0, 0.0}; // zuletzt angezeigte (gefilterte) Position
    uint32_t last_valid_ms_ = 0;
    Sample samples_[3] = {};
    int sample_count_ = 0;
    int rejects_in_a_row_ = 0;
    uint32_t rejected_jumps_ = 0;
};

} // namespace golf
