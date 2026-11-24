// WLAN-Zustand des Geraets, so wie ihn App und Handy-Schnittstelle sehen.
// Die eigentliche Funktechnik steckt hinter dieser Schnittstelle (auf dem
// Geraet WiFi, im Simulator eine Attrappe).
#pragma once

#include <cstdint>

namespace golf {

enum class NetMode : uint8_t {
    Off,        // WLAN aus
    Connecting, // versucht, sich mit dem gespeicherten WLAN zu verbinden
    Hotspot,    // das Geraet spannt ein eigenes WLAN auf (Ersteinrichtung oder Rueckfall)
    Connected,  // im gespeicherten WLAN angemeldet
};

struct NetStatus {
    NetMode mode = NetMode::Off;
    char ssid[33] = "";    // Name des WLANs (eigener Hotspot oder das verbundene)
    char address[40] = ""; // Adresse der Web-Oberflaeche, z. B. "192.168.4.1"
    bool has_saved_network = false;
};

constexpr int kMaxSsidBytes = 32;
constexpr int kMinPasswordBytes = 8; // WPA2; leer = offenes WLAN
constexpr int kMaxPasswordBytes = 63;

class NetworkControl {
public:
    virtual ~NetworkControl() = default;

    virtual NetStatus status() const = 0;

    // Merkt sich das WLAN, mit dem sich das Geraet kuenftig verbindet.
    virtual bool saveNetwork(const char* ssid, const char* password) = 0;

    // Vergisst das gespeicherte WLAN; danach gibt es wieder nur den Hotspot.
    virtual bool forgetNetwork() = 0;
};

} // namespace golf
