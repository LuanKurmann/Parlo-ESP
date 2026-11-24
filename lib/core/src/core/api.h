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
//   GET    /api/rounds/id/<kennung>   dasselbe ueber die dauerhafte Kennung
//   DELETE /api/rounds/id/<kennung>
//   GET    /api/settings      Schlagweiten pro Schlaeger
//   PUT    /api/settings      Schlagweiten setzen
//   GET    /api/wifi          WLAN-Zustand des Geraets (nie das Passwort)
//   PUT    /api/wifi          WLAN merken: {"ssid":"...","password":"..."}
//   DELETE /api/wifi          gespeichertes WLAN vergessen
//
// Alle Inhalte sind JSON in den Formaten aus serialize.h. Fehler kommen als
// {"error":"..."} mit passendem Statuscode.
#pragma once

#include <string>

#include "core/session.h"

namespace golf {

// Version der Schnittstelle. Steigt, wenn sich etwas aendert, auf das sich
// eine App verlassen koennte.
//   1  erste Fassung
//   2  dauerhafte Kennungen fuer Runden, Plaetze und das Geraet
constexpr int kApiVersion = 2;

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

// `network` darf fehlen; dann gibt es /api/wifi nicht (404).
ApiResponse handleApi(Session& session, const ApiRequest& request, NetworkControl* network = nullptr);

} // namespace golf
