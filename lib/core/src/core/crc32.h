// CRC-32 (wie bei ZIP und Ethernet) zum Erkennen beschaedigter Dateien.
#pragma once

#include <cstddef>
#include <cstdint>

namespace golf {

uint32_t crc32(const char* data, size_t length);

} // namespace golf
