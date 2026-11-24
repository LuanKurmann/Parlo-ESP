// Kleiner HTTP-Server fuer den Simulator: liefert die Web-Oberflaeche aus dem
// Ordner web/ und beantwortet /api/... ueber denselben Code wie das Geraet.
// Nur fuer den lokalen Gebrauch gedacht (eine Anfrage nach der anderen).
#pragma once

#include <string>

#include "core/network.h"
#include "core/session.h"

namespace sim {

class HttpServer {
public:
    ~HttpServer();

    // Lauscht auf 127.0.0.1:<port>. `web_dir` enthaelt index.html usw.
    bool start(int port, const std::string& web_dir);

    // Bearbeitet wartende Anfragen, ohne zu blockieren. Liefert true, wenn
    // eine Anfrage den Zustand veraendert haben koennte.
    bool poll(golf::Session& session, golf::NetworkControl* network = nullptr);

private:
    void handleClient(int client, golf::Session& session, golf::NetworkControl* network, bool& changed);

    int listener_ = -1;
    std::string web_dir_;
};

} // namespace sim
