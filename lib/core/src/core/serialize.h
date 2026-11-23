// Platz und Runde als JSON schreiben und lesen. Dasselbe Format dient spaeter
// dem Austausch mit dem Handy.
#pragma once

#include <string>

#include "core/course.h"
#include "core/round.h"

namespace golf {

// Datum und Uhrzeit (UTC, vom GPS).
struct DateTime {
    bool valid = false;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
};

// Eine Runde samt dem, was zum Fortsetzen noetig ist.
struct RoundState {
    Round round;
    int hole = 0;             // aktuelles Loch, 0 = Loch 1
    DateTime started;         // Zeitpunkt des ersten Schlags, falls bekannt
    char course_name[32] = "";
};

std::string courseToJson(const Course& course);

// Liest einen Platz. Bei jedem Fehler (kein JSON, falsche Version, Werte
// ausserhalb des Erlaubten) bleibt `course` unveraendert und es kommt false zurueck.
bool courseFromJson(const std::string& json, Course& course);

std::string roundToJson(const Round& round, int hole, const DateTime& started, const char* course_name);
bool roundFromJson(const std::string& json, RoundState& state);

} // namespace golf
