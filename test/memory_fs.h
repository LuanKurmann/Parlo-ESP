// Dateisystem im Arbeitsspeicher fuer Tests, mit simuliertem Stromausfall.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/fs.h"

namespace testsupport {

class MemoryFs : public golf::FileSystem {
public:
    std::map<std::string, std::string> files;

    // Stromausfall bei der n-ten veraendernden Operation (1 = die naechste).
    // 0 = kein Ausfall. Ab dem Ausfall schlaegt alles fehl, bis reboot().
    int power_cut_at = 0;
    // Was von einem unterbrochenen Schreiben uebrig bleibt: 0.0 = leere Datei,
    // 0.5 = halbe Datei, 1.0 = ganze Datei (Ausfall direkt danach).
    double partial_write = 0.5;
    // Ob eine unterbrochene Umbenennung/Loeschung noch ausgefuehrt wurde.
    bool cut_op_completes = false;
    // Alle veraendernden Operationen schlagen fehl (Karte voll oder entfernt).
    bool read_only = false;

    int mutations = 0; // Zaehler aller veraendernden Operationen
    bool dead = false;

    void reboot() {
        dead = false;
        power_cut_at = 0;
    }

    bool read(const std::string& path, std::string& out) override {
        if (dead) return false;
        auto it = files.find(path);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }

    bool write(const std::string& path, const std::string& data) override {
        if (dead || read_only) return false;
        if (cutNow()) {
            files[path] = data.substr(0, static_cast<size_t>(static_cast<double>(data.size()) * partial_write));
            return false;
        }
        files[path] = data;
        return true;
    }

    bool rename(const std::string& from, const std::string& to) override {
        if (dead || read_only) return false;
        if (files.count(from) == 0 || files.count(to) != 0) return false;
        const bool cut = cutNow();
        if (!cut || cut_op_completes) {
            files[to] = files[from];
            files.erase(from);
        }
        return !cut;
    }

    bool remove(const std::string& path) override {
        if (dead || read_only) return false;
        if (files.count(path) == 0) return false;
        const bool cut = cutNow();
        if (!cut || cut_op_completes) files.erase(path);
        return !cut;
    }

    bool exists(const std::string& path) override { return !dead && files.count(path) != 0; }

    std::vector<std::string> list(const std::string& directory) override {
        std::vector<std::string> names;
        if (dead) return names;
        const std::string prefix = directory + "/";
        for (const auto& entry : files) {
            if (entry.first.compare(0, prefix.size(), prefix) == 0 &&
                entry.first.find('/', prefix.size()) == std::string::npos) {
                names.push_back(entry.first.substr(prefix.size()));
            }
        }
        return names;
    }

private:
    bool cutNow() {
        mutations++;
        if (power_cut_at != 0 && mutations == power_cut_at) {
            dead = true;
            return true;
        }
        return false;
    }
};

} // namespace testsupport
