// Schmale Schnittstelle zum Dateisystem. Dahinter steckt auf dem Geraet die
// SD-Karte oder der interne Flash, im Simulator ein Ordner auf dem PC und in
// den Tests ein Dateisystem im Arbeitsspeicher.
#pragma once

#include <string>
#include <vector>

namespace golf {

class FileSystem {
public:
    virtual ~FileSystem() = default;

    // Liest die ganze Datei. false, wenn sie fehlt oder nicht lesbar ist.
    virtual bool read(const std::string& path, std::string& out) = 0;

    // Schreibt die ganze Datei neu und legt fehlende Ordner an.
    virtual bool write(const std::string& path, const std::string& data) = 0;

    // Benennt um. Darf fehlschlagen, wenn das Ziel schon existiert (wie auf FAT).
    virtual bool rename(const std::string& from, const std::string& to) = 0;

    virtual bool remove(const std::string& path) = 0;
    virtual bool exists(const std::string& path) = 0;

    // Dateinamen (ohne Ordner) direkt in einem Ordner. Leer, wenn er fehlt.
    virtual std::vector<std::string> list(const std::string& directory) = 0;
};

} // namespace golf
