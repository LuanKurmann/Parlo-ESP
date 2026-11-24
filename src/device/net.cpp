#include "net.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "core/api.h"
#include "web_files.h" // erzeugt von scripts/embed_web.py aus dem Ordner web/

namespace device {

namespace {

constexpr uint32_t kConnectTimeoutMs = 15000;
const char kHotspotName[] = "Golf-Tracker";
const char kPrefsNamespace[] = "golf-wifi";

WebServer server(80);
golf::Session* g_session = nullptr;
Net* g_net = nullptr;

const char* methodName(HTTPMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_POST: return "POST";
        default: return "?";
    }
}

void handleRequest() {
    const String uri = server.uri();
    if (uri.startsWith("/api/")) {
        if (g_session == nullptr) {
            server.send(503, "application/json", "{\"error\":\"Nicht bereit\"}");
            return;
        }
        golf::ApiRequest request;
        request.method = methodName(server.method());
        request.path = uri.c_str();
        request.body = server.arg("plain").c_str();
        const golf::ApiResponse response = golf::handleApi(*g_session, request, g_net);
        server.sendHeader("Cache-Control", "no-store");
        server.send(response.status, response.content_type.c_str(), response.body.c_str());
        return;
    }
    const String path = uri == "/" ? String("/index.html") : uri;
    for (size_t i = 0; i < kWebFileCount; i++) {
        if (path == kWebFiles[i].path) {
            server.send_P(200, kWebFiles[i].content_type, reinterpret_cast<const char*>(kWebFiles[i].data), kWebFiles[i].size);
            return;
        }
    }
    server.send(404, "text/plain", "Nicht gefunden");
}

// Passwort des Hotspots: acht Ziffern aus der Geraetenummer, damit es fuer
// jedes Geraet anders, aber immer gleich ist.
void hotspotPassword(char* out, size_t size) {
    const uint64_t mac = ESP.getEfuseMac();
    snprintf(out, size, "%08lu", static_cast<unsigned long>((mac ^ (mac >> 24)) % 100000000ULL));
}

void copy(char* out, size_t size, const String& text) { snprintf(out, size, "%s", text.c_str()); }

} // namespace

void Net::begin() {
    g_net = this;
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    status_.has_saved_network = prefs.getString("ssid", "").length() > 0;
    prefs.end();
    server.onNotFound(handleRequest);
}

bool Net::saveNetwork(const char* ssid, const char* password) {
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) return false;
    const bool ok = prefs.putString("ssid", ssid) > 0;
    prefs.putString("password", password);
    prefs.end();
    if (ok) status_.has_saved_network = true;
    return ok;
}

bool Net::forgetNetwork() {
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) return false;
    prefs.clear();
    prefs.end();
    status_.has_saved_network = false;
    return true;
}

void Net::startHotspot() {
    char password[16];
    hotspotPassword(password, sizeof(password));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kHotspotName, password);
    status_.mode = golf::NetMode::Hotspot;
    copy(status_.ssid, sizeof(status_.ssid), kHotspotName);
    copy(status_.address, sizeof(status_.address), WiFi.softAPIP().toString());
    snprintf(status_.hotspot_password, sizeof(status_.hotspot_password), "%s", password);
    if (!server_running_) {
        server.begin();
        server_running_ = true;
    }
}

void Net::start(uint32_t now_ms) {
    Preferences prefs;
    prefs.begin(kPrefsNamespace, true);
    const String ssid = prefs.getString("ssid", "");
    const String password = prefs.getString("password", "");
    prefs.end();

    status_.hotspot_password[0] = '\0';
    status_.address[0] = '\0';
    if (ssid.length() == 0) {
        startHotspot();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    status_.mode = golf::NetMode::Connecting;
    copy(status_.ssid, sizeof(status_.ssid), ssid);
    connect_started_ms_ = now_ms;
}

void Net::stop() {
    if (server_running_) {
        server.stop();
        server_running_ = false;
    }
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    status_.mode = golf::NetMode::Off;
    status_.ssid[0] = '\0';
    status_.address[0] = '\0';
    status_.hotspot_password[0] = '\0';
}

void Net::loop(golf::Session& session, bool wanted, uint32_t now_ms) {
    g_session = &session;
    if (!wanted) {
        if (status_.mode != golf::NetMode::Off) stop();
        return;
    }
    if (status_.mode == golf::NetMode::Off) start(now_ms);

    if (status_.mode == golf::NetMode::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            status_.mode = golf::NetMode::Connected;
            copy(status_.address, sizeof(status_.address), WiFi.localIP().toString());
            server.begin();
            server_running_ = true;
        } else if (now_ms - connect_started_ms_ >= kConnectTimeoutMs) {
            WiFi.disconnect(true);
            startHotspot(); // WLAN nicht erreichbar: Rueckfall auf den eigenen Hotspot
        }
    }
    if (server_running_) server.handleClient();
}

} // namespace device
