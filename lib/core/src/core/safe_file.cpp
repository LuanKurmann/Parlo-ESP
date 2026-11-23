#include "core/safe_file.h"

#include <cstdio>

#include "core/crc32.h"

namespace golf {

namespace {

const char kMagic[] = "GOLF1 ";
constexpr size_t kMagicLength = sizeof(kMagic) - 1;

bool parseHex8(const std::string& s, size_t pos, uint32_t& out) {
    if (pos + 8 > s.size()) return false;
    uint32_t value = 0;
    for (size_t i = pos; i < pos + 8; i++) {
        const char c = s[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') {
            digit = static_cast<uint32_t>(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<uint32_t>(c - 'A' + 10);
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }
    out = value;
    return true;
}

bool loadOne(FileSystem& fs, const std::string& path, std::string& payload) {
    std::string content;
    return fs.read(path, content) && unwrapPayload(content, payload);
}

} // namespace

std::string wrapPayload(const std::string& payload) {
    char header[40];
    std::snprintf(header, sizeof(header), "%s%08lX %lu\n", kMagic,
                  static_cast<unsigned long>(crc32(payload.data(), payload.size())),
                  static_cast<unsigned long>(payload.size()));
    return std::string(header) + payload;
}

bool unwrapPayload(const std::string& content, std::string& payload) {
    if (content.compare(0, kMagicLength, kMagic) != 0) return false;
    uint32_t expected_crc = 0;
    if (!parseHex8(content, kMagicLength, expected_crc)) return false;

    size_t pos = kMagicLength + 8;
    if (pos >= content.size() || content[pos] != ' ') return false;
    pos++;

    // Laenge: 1 bis 7 Ziffern ohne fuehrende Null (ausser "0").
    size_t length = 0;
    size_t digits = 0;
    while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
        if (digits == 1 && length == 0) return false;
        length = length * 10 + static_cast<size_t>(content[pos] - '0');
        pos++;
        if (++digits > 7) return false;
    }
    if (digits == 0 || pos >= content.size() || content[pos] != '\n') return false;
    pos++;

    if (length > kMaxPayloadBytes || content.size() - pos != length) return false;
    if (crc32(content.data() + pos, length) != expected_crc) return false;
    payload.assign(content, pos, length);
    return true;
}

namespace {

// Schritte 2 und 3: die fertige ".new" an die Stelle der Hauptdatei setzen.
bool promote(FileSystem& fs, const std::string& path) {
    if (fs.exists(path) && !fs.remove(path)) return false;
    return fs.rename(path + ".new", path);
}

} // namespace

bool saveSafely(FileSystem& fs, const std::string& path, const std::string& payload) {
    if (payload.size() > kMaxPayloadBytes) return false;
    if (!fs.write(path + ".new", wrapPayload(payload))) return false;
    return promote(fs, path);
}

bool loadSafely(FileSystem& fs, const std::string& path, std::string& payload) {
    // Eine vollstaendige ".new" ist immer neuer als die Hauptdatei: Sie bleibt
    // nur liegen, wenn der Strom zwischen Schreiben und Umbenennen ausfiel.
    if (loadOne(fs, path + ".new", payload)) {
        promote(fs, path); // gelingt das nicht, wird es beim naechsten Mal nachgeholt
        return true;
    }
    return loadOne(fs, path, payload);
}

} // namespace golf
