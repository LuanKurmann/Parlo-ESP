// I2C-Bus des Geraets hinter der Schnittstelle des Kerns.
#pragma once

#include <Wire.h>

#include "core/i2c.h"

class WireBus : public golf::I2cBus {
public:
    bool write(uint8_t address, const uint8_t* data, size_t length) override {
        Wire.beginTransmission(address);
        Wire.write(data, length);
        return Wire.endTransmission() == 0;
    }

    bool read(uint8_t address, uint8_t* data, size_t length) override {
        // Wire liefert hoechstens einen Pufferinhalt pro Aufruf; grosse Bloecke in Stuecken holen.
        size_t done = 0;
        while (done < length) {
            const size_t chunk = (length - done) > kChunk ? kChunk : (length - done);
            const bool last = done + chunk == length;
            if (Wire.requestFrom(address, chunk, last) != chunk) return false;
            for (size_t i = 0; i < chunk; i++) data[done + i] = static_cast<uint8_t>(Wire.read());
            done += chunk;
        }
        return true;
    }

private:
    static constexpr size_t kChunk = 120;
};
