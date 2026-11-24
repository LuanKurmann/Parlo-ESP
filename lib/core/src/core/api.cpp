#include "core/api.h"

#include <cstdio>
#include <memory>
#include <vector>

#include <ArduinoJson.h>

#include "core/app_info.h"

namespace golf {

namespace {

ApiResponse json(int status, const JsonDocument& doc) {
    ApiResponse response;
    response.status = status;
    serializeJson(doc, response.body);
    return response;
}

ApiResponse error(int status, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    return json(status, doc);
}

ApiResponse raw(const std::string& body) {
    ApiResponse response;
    response.body = body;
    return response;
}

ApiResponse methodNotAllowed() { return error(405, "Methode nicht erlaubt"); }

// "/api/rounds/12" -> 12; 0 bei allem anderen.
int roundNumber(const std::string& path) {
    const std::string prefix = "/api/rounds/";
    if (path.compare(0, prefix.size(), prefix) != 0) return 0;
    const std::string digits = path.substr(prefix.size());
    if (digits.empty() || digits.size() > 4) return 0;
    int number = 0;
    for (char c : digits) {
        if (c < '0' || c > '9') return 0;
        number = number * 10 + (c - '0');
    }
    return number;
}

ApiResponse status(Session& session) {
    JsonDocument doc;
    doc["version"] = version();
    doc["course"] = session.app().course().name;
    doc["holes"] = session.app().round().holeCount();
    doc["current_hole"] = session.app().currentHole() + 1;
    doc["rounds"] = session.store().archivedRounds().size();
    doc["storage_ok"] = session.lastSyncOk();
    return json(200, doc);
}

ApiResponse roundList(Session& session) {
    JsonDocument doc;
    JsonArray list = doc["rounds"].to<JsonArray>();
    const std::vector<int> numbers = session.store().archivedRounds();
    std::unique_ptr<RoundState> state(new RoundState());
    for (auto it = numbers.rbegin(); it != numbers.rend(); ++it) { // neueste zuerst
        if (!session.store().loadArchivedRound(*it, *state)) continue; // beschaedigte Datei auslassen
        const RoundTotals totals = roundTotals(state->round, courseOf(*state));
        JsonObject entry = list.add<JsonObject>();
        entry["n"] = *it;
        if (state->started.valid) {
            char date[24];
            std::snprintf(date, sizeof(date), "%04d-%02d-%02dT%02d:%02d", state->started.year % 10000,
                          state->started.month % 100, state->started.day % 100, state->started.hour % 100,
                          state->started.minute % 100);
            entry["date"] = date;
        }
        entry["course"] = state->course_name;
        entry["holes"] = totals.holes_played;
        entry["strokes"] = totals.strokes;
        entry["putts"] = totals.putts;
        if (totals.holes_with_par > 0 && totals.holes_with_par == totals.holes_played) entry["to_par"] = totals.to_par;
    }
    return json(200, doc);
}

ApiResponse putCourse(Session& session, const std::string& body) {
    std::unique_ptr<Course> course(new Course());
    if (!courseFromJson(body, *course)) return error(400, "Ungueltige Platzdaten");
    bool archived = false;
    if (!session.replaceCourse(*course, &archived)) return error(500, "Speichern fehlgeschlagen");
    JsonDocument doc;
    doc["ok"] = true;
    doc["archived_round"] = archived;
    return json(200, doc);
}

ApiResponse putSettings(Session& session, const std::string& body) {
    Bag bag;
    if (!bagFromJson(body, bag)) return error(400, "Ungueltige Einstellungen");
    if (!session.setBag(bag)) return error(500, "Speichern fehlgeschlagen");
    JsonDocument doc;
    doc["ok"] = true;
    return json(200, doc);
}

} // namespace

ApiResponse handleApi(Session& session, const ApiRequest& request) {
    const std::string& path = request.path;
    const std::string& method = request.method;
    if (request.body.size() > kMaxApiBodyBytes) return error(413, "Anfrage zu gross");

    if (path == "/api/status") {
        return method == "GET" ? status(session) : methodNotAllowed();
    }
    if (path == "/api/course") {
        if (method == "GET") return raw(courseToJson(session.app().course()));
        if (method == "PUT") return putCourse(session, request.body);
        return methodNotAllowed();
    }
    if (path == "/api/round") {
        if (method != "GET") return methodNotAllowed();
        return raw(roundToJson(session.app().round(), session.app().currentHole(), session.roundStarted(),
                               session.app().course().name, &session.app().course()));
    }
    if (path == "/api/rounds") {
        return method == "GET" ? roundList(session) : methodNotAllowed();
    }
    if (path == "/api/settings") {
        if (method == "GET") return raw(bagToJson(session.app().bag()));
        if (method == "PUT") return putSettings(session, request.body);
        return methodNotAllowed();
    }
    if (path.compare(0, 12, "/api/rounds/") == 0) {
        const int number = roundNumber(path);
        if (number == 0) return error(404, "Runde nicht gefunden");
        if (method == "GET") {
            std::string body;
            if (!session.store().loadArchivedRoundJson(number, body)) return error(404, "Runde nicht gefunden");
            return raw(body);
        }
        if (method == "DELETE") {
            if (!session.store().deleteArchivedRound(number)) return error(404, "Runde nicht gefunden");
            JsonDocument doc;
            doc["ok"] = true;
            return json(200, doc);
        }
        return methodNotAllowed();
    }
    return error(404, "Unbekannte Adresse");
}

} // namespace golf
