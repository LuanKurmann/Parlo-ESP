// Stellt das Bluetooth-Protokoll (core/link.h) im Simulator ueber TCP bereit,
// damit eine App ohne Bluetooth dagegen getestet werden kann.
//
// TCP kennt keine Paketgrenzen. Deshalb steht vor jedem Stueck dessen Laenge
// als zwei Bytes (niederwertiges zuerst). Der Simulator sendet Stuecke mit
// hoechstens `packet_size` Bytes, so wie es ueber Bluetooth der Fall waere.
#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "core/link.h"

namespace sim {

class LinkTcp {
public:
    ~LinkTcp();

    // Lauscht auf 127.0.0.1:<port>.
    bool start(int port, size_t packet_size);

    // Bearbeitet Wartendes, ohne zu blockieren. Liefert true, wenn eine
    // Anfrage den Zustand veraendert haben koennte.
    bool poll(golf::Session& session, golf::NetworkControl* network);

private:
    void closeClient();

    int listener_ = -1;
    int client_ = -1;
    size_t packet_size_ = 180;
    std::string buffer_;
    std::unique_ptr<golf::LinkServer> server_;
};

} // namespace sim
