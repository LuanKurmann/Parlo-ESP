// Schmale Schnittstelle zum I2C-Bus. Auf dem Geraet steckt Wire dahinter, in
// den Tests ein nachgebautes Geraet.
#pragma once

#include <cstddef>
#include <cstdint>

namespace golf {

class I2cBus {
public:
    virtual ~I2cBus() = default;

    // Schreibt `length` Bytes an die 7-Bit-Adresse. false bei einem Busfehler.
    virtual bool write(uint8_t address, const uint8_t* data, size_t length) = 0;

    // Liest `length` Bytes von der 7-Bit-Adresse. false bei einem Busfehler.
    virtual bool read(uint8_t address, uint8_t* data, size_t length) = 0;
};

} // namespace golf
