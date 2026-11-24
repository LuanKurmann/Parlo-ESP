// Dateisystem im internen Flash (LittleFS) hinter der Schnittstelle des Kerns.
#pragma once

#include "core/fs.h"

namespace device {

class FlashFileSystem : public golf::FileSystem {
public:
    // Bindet das Dateisystem ein; ein unformatierter Flash wird formatiert.
    bool begin();

    bool read(const std::string& path, std::string& out) override;
    bool write(const std::string& path, const std::string& data) override;
    bool rename(const std::string& from, const std::string& to) override;
    bool remove(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::vector<std::string> list(const std::string& directory) override;
};

} // namespace device
