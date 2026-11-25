#include "core/api.h"

#include <cstdio>
#include <cstring>
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
    doc["api"] = kApiVersion;
    doc["device_id"] = session.deviceId();
    doc["course"] = session.app().course().name;
    doc["course_id"] = session.app().course().id;
    doc["course_rev"] = session.app().course().revision;
    doc["holes"] = session.app().round().holeCount();
    doc["current_hole"] = session.app().currentHole() + 1;
    doc["rounds"] = session.store().archivedRounds().size();
    doc["storage_ok"] = session.lastSyncOk();
    if (session.app().battery().known) {
        doc["battery"] = session.app().battery().percent;
        doc["charging"] = session.app().battery().charging;
    }
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
        entry["id"] = state->id;
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
    Bag bag = session.app().bag(); // was die Anfrage nicht nennt, bleibt
    DeviceSettings device = session.deviceSettings();
    if (!settingsFromJson(body, bag, device)) return error(400, "Ungueltige Einstellungen");
    if (!session.setSettings(bag, device)) return error(500, "Speichern fehlgeschlagen");
    JsonDocument doc;
    doc["ok"] = true;
    return json(200, doc);
}

const char* modeName(NetMode mode) {
    switch (mode) {
        case NetMode::Off: return "off";
        case NetMode::Connecting: return "connecting";
        case NetMode::Hotspot: return "hotspot";
        case NetMode::Connected: return "connected";
    }
    return "off";
}

ApiResponse getWifi(NetworkControl& network) {
    const NetStatus status = network.status();
    JsonDocument doc;
    doc["mode"] = modeName(status.mode);
    doc["ssid"] = status.ssid;
    doc["address"] = status.address;
    doc["saved"] = status.has_saved_network;
    return json(200, doc);
}

// Druckbare Zeichen, keine Steuerzeichen.
bool printable(const char* text) {
    for (const char* p = text; *p != '\0'; p++) {
        if (static_cast<unsigned char>(*p) < 0x20 || *p == 0x7F) return false;
    }
    return true;
}

ApiResponse putWifi(NetworkControl& network, const std::string& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonObjectConst>()) {
        return error(400, "Ungueltige WLAN-Daten");
    }
    if (!doc["ssid"].is<const char*>()) return error(400, "WLAN-Name fehlt");
    const char* ssid = doc["ssid"].as<const char*>();
    const char* password = doc["password"].is<const char*>() ? doc["password"].as<const char*>() : "";
    if (!doc["password"].isNull() && !doc["password"].is<const char*>()) return error(400, "Ungueltiges Passwort");

    const size_t ssid_length = std::strlen(ssid);
    const size_t password_length = std::strlen(password);
    if (ssid_length < 1 || ssid_length > static_cast<size_t>(kMaxSsidBytes) || !printable(ssid)) {
        return error(400, "WLAN-Name muss 1 bis 32 Zeichen haben");
    }
    if (password_length != 0 &&
        (password_length < static_cast<size_t>(kMinPasswordBytes) || password_length > static_cast<size_t>(kMaxPasswordBytes))) {
        return error(400, "Passwort muss 8 bis 63 Zeichen haben (oder leer sein)");
    }
    if (!printable(password)) return error(400, "Ungueltiges Passwort");
    if (!network.saveNetwork(ssid, password)) return error(500, "Speichern fehlgeschlagen");
    JsonDocument answer;
    answer["ok"] = true;
    return json(200, answer);
}

} // namespace

ApiResponse handleApi(Session& session, const ApiRequest& request, NetworkControl* network) {
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
                               session.app().course().name, &session.app().course(), session.roundId()));
    }
    if (path == "/api/rounds") {
        return method == "GET" ? roundList(session) : methodNotAllowed();
    }
    if (path == "/api/settings") {
        if (method == "GET") return raw(settingsToJson(session.app().bag(), session.deviceSettings()));
        if (method == "PUT") return putSettings(session, request.body);
        return methodNotAllowed();
    }
    if (path == "/api/wifi" && network != nullptr) {
        if (method == "GET") return getWifi(*network);
        if (method == "PUT") return putWifi(*network, request.body);
        if (method == "DELETE") {
            if (!network->forgetNetwork()) return error(500, "Speichern fehlgeschlagen");
            JsonDocument doc;
            doc["ok"] = true;
            return json(200, doc);
        }
        return methodNotAllowed();
    }
    if (path.compare(0, 12, "/api/rounds/") == 0) {
        // Entweder ueber die Nummer oder ueber die dauerhafte Kennung.
        const std::string id_prefix = "/api/rounds/id/";
        const int number = path.compare(0, id_prefix.size(), id_prefix) == 0
                               ? session.store().findArchivedRound(path.c_str() + id_prefix.size())
                               : roundNumber(path);
        if (number == 0) return error(404, "Runde nicht gefunden");
        if (method == "GET") {
            // Neu aufbereitet, damit auch aeltere Runden ihre Kennung tragen.
            std::unique_ptr<RoundState> state(new RoundState());
            if (!session.store().loadArchivedRound(number, *state)) return error(404, "Runde nicht gefunden");
            const Course course = courseOf(*state);
            return raw(roundToJson(state->round, state->hole, state->started, state->course_name, &course, state->id));
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
