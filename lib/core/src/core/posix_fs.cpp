#ifndef ARDUINO

#include "core/posix_fs.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace golf {

namespace fsys = std::filesystem;

bool PosixFileSystem::read(const std::string& path, std::string& out) {
    std::ifstream in(full(path), std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad()) return false;
    out = buffer.str();
    return true;
}

bool PosixFileSystem::write(const std::string& path, const std::string& data) {
    std::error_code ec;
    const fsys::path target(full(path));
    if (target.has_parent_path()) fsys::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    return out.good();
}

bool PosixFileSystem::rename(const std::string& from, const std::string& to) {
    std::error_code ec;
    if (fsys::exists(full(to), ec)) return false; // wie auf FAT: kein Ueberschreiben
    return std::rename(full(from).c_str(), full(to).c_str()) == 0;
}

bool PosixFileSystem::remove(const std::string& path) { return std::remove(full(path).c_str()) == 0; }

bool PosixFileSystem::exists(const std::string& path) {
    std::error_code ec;
    return fsys::is_regular_file(full(path), ec);
}

std::vector<std::string> PosixFileSystem::list(const std::string& directory) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fsys::directory_iterator it(full(directory), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) names.push_back(it->path().filename().string());
    }
    return names;
}

} // namespace golf

#endif // ARDUINO
