#include "core/id.h"

#include <cstdio>

#include "core/crc32.h"

namespace golf {

namespace {

uint32_t fallback_counter = 0;

void format(Id out, uint32_t high, uint32_t low) {
    std::snprintf(out, kIdLength + 1, "%08lx%08lx", static_cast<unsigned long>(high), static_cast<unsigned long>(low));
}

// Verteilt die Bits einer Zahl gut (aus MurmurHash3).
uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}

} // namespace

void makeId(Id out, RandomFn random, uint32_t seed) {
    if (random != nullptr) {
        // Zwei getrennte Anweisungen: In einem Funktionsaufruf waere die
        // Reihenfolge der beiden Aufrufe nicht festgelegt.
        const uint32_t high = random();
        const uint32_t low = random();
        format(out, high, low);
        return;
    }
    fallback_counter++;
    format(out, mix(seed ^ 0x9E3779B9u), mix(seed + fallback_counter * 0x7F4A7C15u));
}

void idFromContent(Id out, const std::string& content) {
    const uint32_t a = crc32(content.data(), content.size());
    // Zweite, unabhaengige Haelfte: dieselben Daten mit vorangestelltem Salz.
    const std::string salted = "golf-id:" + content;
    const uint32_t b = crc32(salted.data(), salted.size());
    format(out, mix(a), mix(b ^ static_cast<uint32_t>(content.size())));
}

bool isValidId(const char* text) {
    if (text == nullptr) return false;
    for (int i = 0; i < kIdLength; i++) {
        const char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return text[kIdLength] == '\0';
}

} // namespace golf
