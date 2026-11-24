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
                          const Course* course = nullptr);
    bool loadCurrentRound(RoundState& state);

    // Legt eine beendete Runde im Archiv ab. Liefert ihre Nummer (ab 1), 0 bei Fehler.
    int archiveRound(const Round& round, const DateTime& started, const char* course_name,
                     const Course* course = nullptr);
    std::vector<int> archivedRounds(); // Nummern, aufsteigend
    bool loadArchivedRound(int number, RoundState& state);
    bool loadArchivedRoundJson(int number, std::string& json); // unveraendert, fuer das Handy
    bool deleteArchivedRound(int number);

    // Einstellungen: die Schlagweiten des Spielers.
    bool saveBag(const Bag& bag);
    bool loadBag(Bag& bag);

    // Bereits fertig erzeugte Inhalte speichern (erspart doppeltes Umwandeln).
    bool saveCourseJson(const std::string& json);
    bool saveCurrentRoundJson(const std::string& json);

private:
    FileSystem& fs_;
};

} // namespace golf
