// Platz und Runde als JSON schreiben und lesen. Dasselbe Format dient spaeter
// dem Austausch mit dem Handy.
#pragma once

#include <string>

#include "core/clubs.h"
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
    // Platzdaten zum Zeitpunkt der Runde, damit eine archivierte Runde auch
    // nach einem Platzwechsel ausgewertet werden kann. 0 bzw. false = unbekannt.
    int par[kMaxHoles] = {};
    bool has_green[kMaxHoles] = {};
    LatLon green[kMaxHoles] = {};
};

// Platz, wie er in einer gespeicherten Runde steckt (Name, Par, Gruens).
Course courseOf(const RoundState& state);

std::string courseToJson(const Course& course);

// Liest einen Platz. Bei jedem Fehler (kein JSON, falsche Version, Werte
// ausserhalb des Erlaubten) bleibt `course` unveraendert und es kommt false zurueck.
bool courseFromJson(const std::string& json, Course& course);

// Mit `course` werden Par und Gruenmitte jedes Lochs mitgeschrieben.
std::string roundToJson(const Round& round, int hole, const DateTime& started, const char* course_name,
                        const Course* course = nullptr);
bool roundFromJson(const std::string& json, RoundState& state);

// Schlagweiten pro Schlaeger. {"v":1,"carry":{"Dr":210,"H3":190,...}}
std::string bagToJson(const Bag& bag);
bool bagFromJson(const std::string& json, Bag& bag);

} // namespace golf
