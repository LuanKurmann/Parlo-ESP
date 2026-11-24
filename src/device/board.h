// Anschluesse des Waveshare ESP32-S3-Touch-AMOLED-1.75 (Variante G mit GPS).
//
// Quelle: offizielles Repository waveshareteam/ESP32-S3-Touch-AMOLED-1.75,
// Datei examples/arduino/libraries/Mylibrary/pin_config.h und
// HARDWARE_REFERENCE.md (Stand November 2026). Am echten Geraet noch nicht
// geprueft; siehe docs/inbetriebnahme.md.
#pragma once

#include <cstdint>

namespace board {

// AMOLED mit Treiber CO5300 ueber QSPI
constexpr int kLcdSdio0 = 4;
constexpr int kLcdSdio1 = 5;
constexpr int kLcdSdio2 = 6;
constexpr int kLcdSdio3 = 7;
constexpr int kLcdSclk = 38;
constexpr int kLcdCs = 12;
constexpr int kLcdReset = 39;
constexpr int kLcdColumnOffset = 6; // sichtbarer Bereich beginnt 6 Spalten nach dem Panel-Anfang

// Gemeinsamer I2C-Bus: Touch, I/O-Baustein, GPS, Stromversorgung, Uhr, Lagesensor
constexpr int kI2cSda = 15;
constexpr int kI2cScl = 14;
constexpr uint32_t kI2cHz = 100000; // wie im GPS-Beispiel des Herstellers

// Touch-Controller CST9217
constexpr uint8_t kTouchAddress = 0x5A;
constexpr int kTouchInt = 11;
constexpr int kTouchReset = 40;

// I/O-Baustein TCA9554: liefert u. a. den Zustand des PWR-Knopfs
constexpr uint8_t kExpanderAddress = 0x20;
constexpr uint8_t kExpanderInputRegister = 0x00;
constexpr uint8_t kExpanderPwrButtonBit = 4; // EXIO4, 1 = gedrueckt
constexpr uint8_t kExpanderGpsResetBit = 7;  // EXIO7, bleibt unangetastet

// Knopf BOOT direkt am Prozessor, 0 = gedrueckt
constexpr int kBootButton = 0;

// GPS-Modul LC76G: Adressen stehen in core/lc76g.h (0x50 schreiben, 0x54 lesen)

} // namespace board
