// Uebertragung der Schnittstelle ueber Verbindungen mit kleinen Paketen
// (Bluetooth Low Energy; im Simulator TCP). Beschreibung fuer App-Entwickler:
// docs/bluetooth-protokoll.md.
//
// Eine Nachricht (Anfrage oder Antwort) ist Text:
//
//   Anfrage   "<nr>\n<METHODE> <pfad>\n<inhalt>"     z. B. "7\nGET /api/status\n"
//   Antwort   "<nr>\n<status>\n<inhalt>"             z. B. "7\n200\n{...}"
//
// Die Nummer waehlt die App; die Antwort traegt dieselbe. Nachrichten werden
// in Stuecke zerlegt, die in ein Paket passen:
//
//   Byte 0      Art: 1 = erstes Stueck, 2 = weiteres Stueck
//   Byte 1-2    laufende Nummer des Stuecks, beginnend bei 0 (niederwertiges Byte zuerst)
//   erstes Stueck zusaetzlich:
//   Byte 3-6    Laenge der ganzen Nachricht in Bytes
//   Byte 7-10   CRC-32 der ganzen Nachricht
//   danach      Nutzdaten
//
// Der Empfaenger setzt die Stuecke zusammen und prueft Reihenfolge, Laenge
// und Pruefsumme. Geht etwas schief, verwirft er die Nachricht.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/api.h"

namespace golf {

// Bluetooth-Kennungen des Dienstes: Die App schreibt Stuecke von Anfragen in
// "Request" und erhaelt Stuecke von Antworten als Benachrichtigung auf "Response".
constexpr char kLinkServiceUuid[] = "7d2f0001-5a3b-4c6e-9b1d-3f0a6c8e4b21";
constexpr char kLinkRequestUuid[] = "7d2f0002-5a3b-4c6e-9b1d-3f0a6c8e4b21";
constexpr char kLinkResponseUuid[] = "7d2f0003-5a3b-4c6e-9b1d-3f0a6c8e4b21";

constexpr uint8_t kLinkFirst = 1;
constexpr uint8_t kLinkNext = 2;
constexpr size_t kLinkFirstHeader = 11;
constexpr size_t kLinkNextHeader = 3;
constexpr size_t kLinkMinFragment = 20;          // kleinstes Bluetooth-Paket
constexpr size_t kLinkMaxMessage = 128 * 1024;   // groesste Nachricht, die angenommen wird

// Zerlegt eine Nachricht in Stuecke.
class LinkFragmenter {
public:
    void start(const std::string& message);
    void reset();
    bool pending() const { return active_; }

    // Liefert das naechste Stueck mit hoechstens `max_size` Bytes (mindestens
    // kLinkMinFragment). false, wenn nichts mehr zu senden ist.
    bool next(size_t max_size, std::string& fragment);

private:
    std::string message_;
    size_t offset_ = 0;
    uint16_t sequence_ = 0;
    bool active_ = false;
};

// Setzt Stuecke wieder zu einer Nachricht zusammen.
class LinkReassembler {
public:
    enum class Result : uint8_t { Incomplete, Complete, Error };

    // Nimmt ein Stueck entgegen. Bei Complete steht die Nachricht in message().
    // Nach Error wird alles bis zum naechsten ersten Stueck verworfen.
    Result add(const uint8_t* data, size_t length);

    const std::string& message() const { return message_; }
    void reset();
    uint32_t errors() const { return errors_; }

private:
    Result fail();

    std::string message_;
    uint32_t total_ = 0;
    uint32_t crc_ = 0;
    uint16_t expected_sequence_ = 0;
    bool receiving_ = false;
    uint32_t errors_ = 0;
};

// Anfragen und Antworten als Text.
std::string encodeLinkRequest(uint32_t id, const ApiRequest& request);
bool decodeLinkRequest(const std::string& message, uint32_t& id, ApiRequest& request);
std::string encodeLinkResponse(uint32_t id, const ApiResponse& response);
bool decodeLinkResponse(const std::string& message, uint32_t& id, int& status, std::string& body);

// Gegenstelle auf dem Geraet: nimmt Stuecke von Anfragen entgegen, beantwortet
// die Anfrage ueber handleApi und stellt die Antwort in Stuecken bereit.
class LinkServer {
public:
    explicit LinkServer(Session& session, NetworkControl* network = nullptr) : session_(session), network_(network) {}

    // Ein empfangenes Stueck. Liefert true, wenn damit eine Anfrage
    // ausgefuehrt wurde (der Zustand des Geraets kann sich geaendert haben).
    bool receive(const uint8_t* data, size_t length);

    // Naechstes zu sendendes Stueck der Antwort, falls eines ansteht.
    bool nextOutgoing(size_t max_size, std::string& fragment) { return outgoing_.next(max_size, fragment); }
    bool hasOutgoing() const { return outgoing_.pending(); }

    // Verbindung getrennt: Angefangenes verwerfen.
    void disconnect();

    uint32_t handledRequests() const { return handled_; }

private:
    void respondError(uint32_t id, int status, const char* message);

    Session& session_;
    NetworkControl* network_;
    LinkReassembler incoming_;
    LinkFragmenter outgoing_;
    bool error_reported_ = false;
    uint32_t handled_ = 0;
};

} // namespace golf
