#include "storage.h"

#include <LittleFS.h>

namespace device {

namespace {
// Der Kern arbeitet mit Pfaden ohne fuehrenden Schraegstrich.
String full(const std::string& path) { return String("/") + path.c_str(); }
} // namespace

bool FlashFileSystem::begin() { return LittleFS.begin(true /* bei Bedarf formatieren */); }

bool FlashFileSystem::read(const std::string& path, std::string& out) {
    File file = LittleFS.open(full(path), "r");
    if (!file || file.isDirectory()) return false;
    const size_t size = file.size();
    std::string data(size, '\0');
    const size_t got = size == 0 ? 0 : file.read(reinterpret_cast<uint8_t*>(&data[0]), size);
    file.close();
    if (got != size) return false;
    out.swap(data);
    return true;
}

bool FlashFileSystem::write(const std::string& path, const std::string& data) {
    const size_t slash = path.rfind('/');
    if (slash != std::string::npos) LittleFS.mkdir(full(path.substr(0, slash)));
    File file = LittleFS.open(full(path), "w");
    if (!file) return false;
    const size_t written = data.empty() ? 0 : file.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    file.close();
    return written == data.size();
}

bool FlashFileSystem::rename(const std::string& from, const std::string& to) {
    if (LittleFS.exists(full(to))) return false; // gleiches Verhalten wie in den Tests
    return LittleFS.rename(full(from), full(to));
}

bool FlashFileSystem::remove(const std::string& path) { return LittleFS.remove(full(path)); }

bool FlashFileSystem::exists(const std::string& path) {
    File file = LittleFS.open(full(path), "r");
    const bool ok = file && !file.isDirectory();
    if (file) file.close();
    return ok;
}

std::vector<std::string> FlashFileSystem::list(const std::string& directory) {
    std::vector<std::string> names;
    File dir = LittleFS.open(full(directory));
    if (!dir || !dir.isDirectory()) return names;
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        if (!entry.isDirectory()) names.push_back(entry.name());
        entry.close();
    }
    return names;
}

} // namespace device
