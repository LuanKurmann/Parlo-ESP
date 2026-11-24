// WLAN und Webserver des Geraets.
//
// Das WLAN laeuft nur, solange auf dem Geraet "Mit Handy verbinden" offen ist.
// Ist ein WLAN gespeichert, meldet sich das Geraet dort an; gelingt das nicht
// oder ist keines gespeichert, spannt es einen eigenen Hotspot auf.
#pragma once

#include "core/network.h"
#include "core/session.h"

namespace device {

class Net : public golf::NetworkControl {
public:
    void begin();

    // In jedem Durchlauf aufrufen. `wanted` sagt, ob das WLAN laufen soll.
    void loop(golf::Session& session, bool wanted, uint32_t now_ms);

    golf::NetStatus status() const override { return status_; }
    bool saveNetwork(const char* ssid, const char* password) override;
    bool forgetNetwork() override;

private:
    void start(uint32_t now_ms);
    void startHotspot();
    void stop();

    golf::NetStatus status_;
    uint32_t connect_started_ms_ = 0;
    bool server_running_ = false;
};

} // namespace device
