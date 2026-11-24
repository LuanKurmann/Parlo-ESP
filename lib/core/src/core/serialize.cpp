#include "core/serialize.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <ArduinoJson.h>

namespace golf {

namespace {

constexpr int kFormatVersion = 1;

// Namen duerfen nur druckbare Zeichen enthalten und muessen in den Puffer passen.
bool copyName(JsonVariantConst value, char* out, size_t capacity) {
    if (value.isNull()) {
        out[0] = '\0';
        return true;
    }
    if (!value.is<const char*>()) return false;
    const char* text = value.as<const char*>();
    const size_t length = std::strlen(text);
    if (length >= capacity) return false;
    for (size_t i = 0; i < length; i++) {
        if (static_cast<unsigned char>(text[i]) < 0x20) return false;
    }
    std::memcpy(out, text, length + 1);
    return true;
}

// Koordinaten stehen als Ganzzahlen in Zehnmillionstel Grad in der Datei
// (47.3769 -> 473769000). Das ist auf rund 1 cm genau und, anders als
// Kommazahlen, beim Schreiben und Lesen verlustfrei.
constexpr double kE7 = 1e7;

void writePosition(JsonObject object, const char* key, const LatLon& p) {
    JsonArray a = object[key].to<JsonArray>();
    a.add(static_cast<int32_t>(std::llround(p.lat * kE7)));
    a.add(static_cast<int32_t>(std::llround(p.lon * kE7)));
}

// Liest [Breite, Laenge]. `present` sagt, ob der Schluessel ueberhaupt da war.
bool readPosition(JsonObjectConst object, const char* key, bool& present, LatLon& out) {
    JsonVariantConst value = object[key];
    present = !value.isNull();
    if (!present) return true;
    JsonArrayConst a = value.as<JsonArrayConst>();
    if (a.isNull() || a.size() != 2) return false;
    if (!a[0].is<int32_t>() || !a[1].is<int32_t>()) return false;
    const int32_t lat = a[0].as<int32_t>();
    const int32_t lon = a[1].as<int32_t>();
    if (lat < -900000000 || lat > 900000000 || lon < -1800000000 || lon > 1800000000) return false;
    out = LatLon{lat / kE7, lon / kE7};
    return true;
}

// Liest eine Kennung, falls vorhanden. Eine vorhandene muss gueltig sein.
bool readId(JsonVariantConst value, Id out) {
    out[0] = '\0';
    if (value.isNull()) return true;
    if (!value.is<const char*>() || !isValidId(value.as<const char*>())) return false;
    std::memcpy(out, value.as<const char*>(), kIdLength + 1);
    return true;
}

bool readInt(JsonVariantConst value, int min, int max, int& out) {
    if (!value.is<int>()) return false;
    const int v = value.as<int>();
    if (v < min || v > max) return false;
    out = v;
    return true;
}

bool parseDocument(const std::string& json, JsonDocument& doc) {
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    if (!doc.is<JsonObjectConst>()) return false;
    int version = 0;
    return readInt(doc["v"], kFormatVersion, kFormatVersion, version);
}

bool readDateTime(const char* text, DateTime& out) {
    DateTime d;
    char tail = '\0';
    if (std::strlen(text) != 16 ||
        std::sscanf(text, "%4d-%2d-%2dT%2d:%2d%c", &d.year, &d.month, &d.day, &d.hour, &d.minute, &tail) != 5) {
        return false;
    }
    if (d.year < 2000 || d.year > 2099 || d.month < 1 || d.month > 12 || d.day < 1 || d.day > 31 || d.hour < 0 ||
        d.hour > 23 || d.minute < 0 || d.minute > 59) {
        return false;
    }
    d.valid = true;
    out = d;
    return true;
}

} // namespace

// ---------- Platz ----------
//
// {"v":1,"name":"Beispielplatz","holes":[{"par":4,"green":[473800000,85400000],"r":130,"tee":[473700000,85400000]}, ...]}
// "green" und "tee" in Zehnmillionstel Grad, "r" (Radius des Gruens) in Dezimetern.
// "green", "r" und "tee" fehlen, wenn sie nicht bekannt sind.

std::string courseToJson(const Course& course) {
    JsonDocument doc;
    doc["v"] = kFormatVersion;
    if (course.id[0] != '\0') {
        doc["id"] = course.id;
        doc["rev"] = course.revision;
    }
    doc["name"] = course.name;
    JsonArray holes = doc["holes"].to<JsonArray>();
    for (int h = 0; h < course.hole_count && h < kMaxHoles; h++) {
        const Hole& hole = course.holes[h];
        JsonObject o = holes.add<JsonObject>();
        o["par"] = hole.par;
        if (hole.has_green) {
            writePosition(o, "green", hole.green_center);
            o["r"] = static_cast<int>(std::lround(hole.green_radius_m * 10.0f));
        }
        if (hole.has_tee) writePosition(o, "tee", hole.tee);
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool courseFromJson(const std::string& json, Course& course) {
    JsonDocument doc;
    if (!parseDocument(json, doc)) return false;

    Course result;
    if (!copyName(doc["name"], result.name, sizeof(result.name))) return false;
    if (!readId(doc["id"], result.id)) return false;
    if (!doc["rev"].isNull()) {
        int revision = 0;
        if (!readInt(doc["rev"], 0, 2000000000, revision)) return false;
        result.revision = static_cast<uint32_t>(revision);
    }

    JsonArrayConst holes = doc["holes"].as<JsonArrayConst>();
    if (holes.isNull() || holes.size() < 1 || holes.size() > static_cast<size_t>(kMaxHoles)) return false;
    result.hole_count = static_cast<int>(holes.size());

    int h = 0;
    for (JsonVariantConst item : holes) {
        JsonObjectConst o = item.as<JsonObjectConst>();
        if (o.isNull()) return false;
        Hole& hole = result.holes[h++];

        if (!readInt(o["par"], 0, 6, hole.par) || hole.par == 1 || hole.par == 2) return false;
        if (!readPosition(o, "green", hole.has_green, hole.green_center)) return false;
        if (!readPosition(o, "tee", hole.has_tee, hole.tee)) return false;

        JsonVariantConst radius = o["r"];
        if (!radius.isNull()) {
            int decimetres = 0;
            if (!readInt(radius, 10, 600, decimetres)) return false;
            hole.green_radius_m = static_cast<float>(decimetres) / 10.0f;
        }
    }
    course = result;
    return true;
}

// ---------- Runde ----------
//
// {"v":1,"course":"Beispielplatz","hole":6,"date":"2026-11-23T08:15",
//  "holes":[{"done":true,"shots":[{"p":[473700000,85400000],"c":"Dr"},{"c":"Pt"}]}, ...]}
// "p" fehlt bei Schlaegen ohne GPS, "c" solange kein Schlaeger gewaehlt ist.

std::string roundToJson(const Round& round, int hole, const DateTime& started, const char* course_name,
                        const Course* course, const char* round_id) {
    JsonDocument doc;
    doc["v"] = kFormatVersion;
    if (round_id != nullptr && round_id[0] != '\0') doc["id"] = round_id;
    if (course != nullptr && course->id[0] != '\0') doc["course_id"] = course->id;
    doc["course"] = course_name != nullptr ? course_name : "";
    doc["hole"] = hole;
    if (started.valid) {
        char text[40];
        std::snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d", started.year % 10000, started.month % 100,
                      started.day % 100, started.hour % 100, started.minute % 100);
        doc["date"] = text;
    }
    JsonArray holes = doc["holes"].to<JsonArray>();
    for (int h = 0; h < round.holeCount(); h++) {
        JsonObject o = holes.add<JsonObject>();
        if (course != nullptr) {
            if (course->holes[h].par > 0) o["par"] = course->holes[h].par;
            if (course->holes[h].has_green) writePosition(o, "green", course->holes[h].green_center);
        }
        o["done"] = round.isFinished(h);
        JsonArray shots = o["shots"].to<JsonArray>();
        for (int i = 0; i < round.shotCount(h); i++) {
            const Shot* s = round.shot(h, i);
            JsonObject so = shots.add<JsonObject>();
            if (s->has_position) writePosition(so, "p", s->position);
            if (s->club != Club::None) so["c"] = clubShortName(s->club);
        }
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool roundFromJson(const std::string& json, RoundState& state) {
    JsonDocument doc;
    if (!parseDocument(json, doc)) return false;

    JsonArrayConst holes = doc["holes"].as<JsonArrayConst>();
    if (holes.isNull() || holes.size() < 1 || holes.size() > static_cast<size_t>(kMaxHoles)) return false;
    const int hole_count = static_cast<int>(holes.size());

    RoundState result;
    result.round = Round(hole_count);
    if (!copyName(doc["course"], result.course_name, sizeof(result.course_name))) return false;
    if (!readId(doc["id"], result.id) || !readId(doc["course_id"], result.course_id)) return false;
    if (!readInt(doc["hole"], 0, hole_count - 1, result.hole)) return false;

    JsonVariantConst date = doc["date"];
    if (!date.isNull()) {
        if (!date.is<const char*>() || !readDateTime(date.as<const char*>(), result.started)) return false;
    }

    int h = 0;
    for (JsonVariantConst item : holes) {
        JsonObjectConst o = item.as<JsonObjectConst>();
        if (o.isNull() || !o["done"].is<bool>()) return false;
        if (!o["par"].isNull()) {
            if (!readInt(o["par"], 3, 6, result.par[h])) return false;
        }
        if (!readPosition(o, "green", result.has_green[h], result.green[h])) return false;
        JsonArrayConst shots = o["shots"].as<JsonArrayConst>();
        if (shots.isNull() || shots.size() > static_cast<size_t>(kMaxShotsPerHole)) return false;

        int index = 0;
        for (JsonVariantConst shot_item : shots) {
            JsonObjectConst so = shot_item.as<JsonObjectConst>();
            if (so.isNull()) return false;

            bool has_position = false;
            LatLon position{0.0, 0.0};
            if (!readPosition(so, "p", has_position, position)) return false;
            if (has_position) {
                result.round.addShot(h, position);
            } else {
                result.round.addShotWithoutPosition(h);
            }

            JsonVariantConst club = so["c"];
            if (!club.isNull()) {
                if (!club.is<const char*>()) return false;
                const Club c = clubFromShortName(club.as<const char*>());
                if (c == Club::None) return false;
                result.round.setClub(h, index, c);
            }
            index++;
        }
        if (o["done"].as<bool>()) {
            if (!result.round.finishHole(h)) return false; // abgeschlossen ohne Schlag gibt es nicht
        }
        h++;
    }
    state = result;
    return true;
}

Course courseOf(const RoundState& state) {
    Course course;
    std::memcpy(course.name, state.course_name, sizeof(course.name));
    course.name[sizeof(course.name) - 1] = '\0';
    std::memcpy(course.id, state.course_id, sizeof(course.id));
    course.hole_count = state.round.holeCount();
    for (int h = 0; h < course.hole_count; h++) {
        course.holes[h].par = state.par[h];
        course.holes[h].has_green = state.has_green[h];
        course.holes[h].green_center = state.green[h];
    }
    return course;
}

// ---------- Schlaegertasche ----------

std::string bagToJson(const Bag& bag) {
    JsonDocument doc;
    doc["v"] = kFormatVersion;
    JsonObject carry = doc["carry"].to<JsonObject>();
    for (int i = 0; i < kClubCount; i++) {
        const Club club = static_cast<Club>(i);
        if (club == Club::Putter) continue; // der Putter hat keine Schlagweite
        carry[clubShortName(club)] = bag.carry_m[i];
    }
    JsonArray clubs = doc["clubs"].to<JsonArray>();
    Club list[kMaxBagClubs];
    const int count = bagClubs(bag, list);
    for (int i = 0; i < count; i++) clubs.add(clubShortName(list[i]));
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool bagFromJson(const std::string& json, Bag& bag) {
    JsonDocument doc;
    if (!parseDocument(json, doc)) return false;
    JsonVariantConst carry_value = doc["carry"];
    JsonVariantConst clubs_value = doc["clubs"];
    if (carry_value.isNull() && clubs_value.isNull()) return false;

    Bag result = bag;
    if (!carry_value.isNull()) {
        JsonObjectConst carry = carry_value.as<JsonObjectConst>();
        if (carry.isNull()) return false;
        for (JsonPairConst entry : carry) {
            const Club club = clubFromShortName(entry.key().c_str());
            if (club == Club::None || club == Club::Putter) return false; // unbekannter Schlaeger
            if (!readInt(entry.value(), 1, 400, result.carry_m[static_cast<int>(club)])) return false;
        }
    }
    if (!clubs_value.isNull()) {
        JsonArrayConst clubs = clubs_value.as<JsonArrayConst>();
        if (clubs.isNull()) return false;
        for (int i = 0; i < kClubCount; i++) result.in_bag[i] = false;
        for (JsonVariantConst item : clubs) {
            if (!item.is<const char*>()) return false;
            const Club club = clubFromShortName(item.as<const char*>());
            if (club == Club::None) return false;
            if (result.in_bag[static_cast<int>(club)]) return false; // doppelt genannt
            result.in_bag[static_cast<int>(club)] = true;
        }
        result.in_bag[static_cast<int>(Club::Putter)] = true; // immer dabei
    }
    if (!isValidBag(result)) return false;
    bag = result;
    return true;
}

} // namespace golf
