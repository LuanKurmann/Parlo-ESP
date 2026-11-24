// Liest die NMEA-Daten des GPS-Moduls LC76G ueber I2C.
//
// Ablauf nach dem Beispiel des Herstellers (Waveshare, 09_LC76G_I2C):
//   1. An 0x50 schreiben: 08 00 51 AA 04 00 00 00   ("wie viele Bytes liegen bereit?")
//   2. 100 ms warten, 4 Bytes von 0x54 lesen: Anzahl als 32-Bit-Zahl, niederwertiges Byte zuerst
//   3. 100 ms warten, an 0x50 schreiben: 00 20 51 AA + dieselben 4 Bytes ("gib sie mir")
//   4. 100 ms warten, so viele Bytes von 0x54 lesen: das ist der NMEA-Text
//
// Die Wartezeiten blockieren nicht: poll() wird in jedem Durchlauf der
// Hauptschleife aufgerufen und macht jeweils hoechstens einen Schritt.
#pragma once

#include <cstdint>
#include <string>

#include "core/i2c.h"

namespace golf {

class Lc76gReader {
public:
    static constexpr uint8_t kWriteAddress = 0x50;
    static constexpr uint8_t kReadAddress = 0x54;
    static constexpr uint32_t kStepDelayMs = 100;   // Pause zwischen den Schritten
    static constexpr uint32_t kIdleDelayMs = 300;   // Pause nach einem vollstaendigen Durchgang
    static constexpr uint32_t kErrorDelayMs = 1000; // Pause nach einem Fehler
    // Groesster Block, der gelesen wird. Was das Modul darueber meldet, gilt als Lesefehler.
    static constexpr uint32_t kMaxChunkBytes = 8192;

    explicit Lc76gReader(I2cBus& bus) : bus_(bus) {}

    // Macht den naechsten faelligen Schritt. Liefert true, wenn `out` neuen
    // NMEA-Text enthaelt (`out` wird dann ersetzt).
    bool poll(uint32_t now_ms, std::string& out);

    uint32_t errors() const { return errors_; }
    uint32_t chunks() const { return chunks_; }

private:
    enum class State : uint8_t { AskLength, ReadLength, RequestData, ReadData };

    void fail(uint32_t now_ms);
    void waitFor(uint32_t now_ms, uint32_t delay_ms, State next);

    I2cBus& bus_;
    State state_ = State::AskLength;
    bool started_ = false;
    uint32_t wait_from_ = 0;
    uint32_t wait_ms_ = 0;
    uint8_t length_bytes_[4] = {};
    uint32_t length_ = 0;
    uint32_t errors_ = 0;
    uint32_t chunks_ = 0;
};

} // namespace golf
