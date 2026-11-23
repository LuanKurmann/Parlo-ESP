// Dateisystem in einem Ordner auf dem PC (fuer den Simulator).
#pragma once

#ifndef ARDUINO

#include "core/fs.h"

namespace golf {

class PosixFileSystem : public FileSystem {
public:
    // Alle Pfade liegen unterhalb von `root`.
    explicit PosixFileSystem(const std::string& root) : root_(root) {}

    bool read(const std::string& path, std::string& out) override;
    bool write(const std::string& path, const std::string& data) override;
    bool rename(const std::string& from, const std::string& to) override;
    bool remove(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::vector<std::string> list(const std::string& directory) override;

private:
    std::string full(const std::string& path) const { return root_ + "/" + path; }

    std::string root_;
};

} // namespace golf

#endif // ARDUINO
