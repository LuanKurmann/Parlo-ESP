// Schnittstelle zum Handy. Nimmt eine Anfrage entgegen und liefert die Antwort,
// unabhaengig davon, wie sie uebertragen wird (WLAN auf dem Geraet, lokaler
// Server im Simulator, direkter Aufruf im Test).
//
//   GET    /api/status        Version, Platzname, Anzahl Runden
//   GET    /api/course        aktueller Platz
//   PUT    /api/course        Platz ersetzen (eine laufende Runde wird archiviert)
//   GET    /api/round         laufende Runde
//   GET    /api/rounds        Liste der archivierten Runden mit Ergebnis
//   GET    /api/rounds/<nr>   eine archivierte Runde
//   DELETE /api/rounds/<nr>   eine archivierte Runde loeschen
//   GET    /api/settings      Schlagweiten pro Schlaeger
//   PUT    /api/settings      Schlagweiten setzen
//
// Alle Inhalte sind JSON in den Formaten aus serialize.h. Fehler kommen als
// {"error":"..."} mit passendem Statuscode.
#pragma once

#include <string>

#include "core/session.h"

namespace golf {

// Groesste Anfrage, die angenommen wird.
constexpr size_t kMaxApiBodyBytes = 64 * 1024;

struct ApiRequest {
    std::string method; // "GET", "PUT", "DELETE"
    std::string path;   // ohne Abfrageteil, z. B. "/api/rounds/3"
    std::string body;
};

struct ApiResponse {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
};

ApiResponse handleApi(Session& session, const ApiRequest& request);

} // namespace golf
