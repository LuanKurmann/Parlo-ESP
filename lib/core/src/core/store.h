// Legt Platz, laufende Runde und beendete Runden im Dateisystem ab.
#pragma once

#include <vector>

#include "core/fs.h"
#include "core/serialize.h"

namespace golf {

class Store {
public:
    explicit Store(FileSystem& fs) : fs_(fs) {}

    bool saveCourse(const Course& course);
    bool loadCourse(Course& course);

    // Die laufende Runde, damit sie einen Neustart uebersteht.
    bool saveCurrentRound(const Round& round, int hole, const DateTime& started, const char* course_name,
                          const Course* course = nullptr, const char* round_id = nullptr);
    bool loadCurrentRound(RoundState& state);

    // Legt eine beendete Runde im Archiv ab. Liefert ihre Nummer (ab 1), 0 bei Fehler.
    int archiveRound(const Round& round, const DateTime& started, const char* course_name,
                     const Course* course = nullptr, const char* round_id = nullptr);
    std::vector<int> archivedRounds(); // Nummern, aufsteigend
    bool loadArchivedRound(int number, RoundState& state);
    bool loadArchivedRoundJson(int number, std::string& json); // so wie gespeichert
    // Nummer der archivierten Runde mit dieser Kennung, 0 wenn es keine gibt.
    int findArchivedRound(const char* id);
    bool deleteArchivedRound(int number);

    // Einstellungen: Bag, Display und Knoepfe in einer Datei. Beim Laden
    // bleibt unveraendert, was in der Datei fehlt (aeltere Fassungen).
    bool saveSettings(const Bag& bag, const DeviceSettings& device);
    bool loadSettings(Bag& bag, DeviceSettings& device);

    // Kennung dieses Geraets; wird einmal erzeugt und bleibt dann gleich.
    bool saveDeviceId(const char* id);
    bool loadDeviceId(Id out);

    // Bereits fertig erzeugte Inhalte speichern (erspart doppeltes Umwandeln).
    bool saveCourseJson(const std::string& json);
    bool saveCurrentRoundJson(const std::string& json);

private:
    FileSystem& fs_;
};

} // namespace golf
