#include "link_tcp.h"

#include <cerrno>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sim {

namespace {

void setNonBlocking(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

// Schreibt alles, auch wenn der Sendepuffer kurz voll ist.
bool sendAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<size_t>(n);
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(1000);
        } else {
            return false;
        }
    }
    return true;
}

} // namespace

LinkTcp::~LinkTcp() {
    closeClient();
    if (listener_ >= 0) close(listener_);
}

bool LinkTcp::start(int port, size_t packet_size) {
    packet_size_ = packet_size < golf::kLinkMinFragment ? golf::kLinkMinFragment : packet_size;
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ < 0) return false;
    const int yes = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener_, 4) != 0) {
        close(listener_);
        listener_ = -1;
        return false;
    }
    setNonBlocking(listener_);
    return true;
}

void LinkTcp::closeClient() {
    if (client_ >= 0) close(client_);
    client_ = -1;
    buffer_.clear();
    if (server_) server_->disconnect();
}

bool LinkTcp::poll(golf::Session& session, golf::NetworkControl* network) {
    if (listener_ < 0) return false;
    if (!server_) server_.reset(new golf::LinkServer(session, network));

    // Eine neue Verbindung ersetzt die bisherige (wie ein neu verbundenes Handy).
    const int incoming = accept(listener_, nullptr, nullptr);
    if (incoming >= 0) {
        closeClient();
        client_ = incoming;
        setNonBlocking(client_);
        const int yes = 1;
        setsockopt(client_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    }
    if (client_ < 0) return false;

    char chunk[4096];
    for (;;) {
        const ssize_t n = recv(client_, chunk, sizeof(chunk), 0);
        if (n > 0) {
            buffer_.append(chunk, static_cast<size_t>(n));
        } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            closeClient(); // Gegenseite hat getrennt
            return false;
        } else {
            break;
        }
    }

    bool changed = false;
    // Vollstaendige Stuecke herausloesen: zwei Bytes Laenge, dann das Stueck.
    while (buffer_.size() >= 2) {
        const size_t length = static_cast<uint8_t>(buffer_[0]) | (static_cast<size_t>(static_cast<uint8_t>(buffer_[1])) << 8);
        if (buffer_.size() < 2 + length) break;
        if (server_->receive(reinterpret_cast<const uint8_t*>(buffer_.data() + 2), length)) changed = true;
        buffer_.erase(0, 2 + length);

        std::string fragment;
        while (server_->nextOutgoing(packet_size_, fragment)) {
            std::string frame;
            frame.push_back(static_cast<char>(fragment.size() & 0xFF));
            frame.push_back(static_cast<char>(fragment.size() >> 8));
            frame += fragment;
            if (!sendAll(client_, frame)) {
                closeClient();
                return changed;
            }
        }
    }
    return changed;
}

} // namespace sim
