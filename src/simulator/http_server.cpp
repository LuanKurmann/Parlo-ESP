#include "http_server.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/api.h"

namespace sim {

namespace {

const char* reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        default: return "Error";
    }
}

const char* contentType(const std::string& path) {
    const size_t dot = path.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : path.substr(dot);
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

void sendAll(int client, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = send(client, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

void respond(int client, int status, const std::string& type, const std::string& body) {
    char head[256];
    std::snprintf(head, sizeof(head),
                  "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                  "Connection: close\r\n\r\n",
                  status, reason(status), type.c_str(), body.size());
    sendAll(client, head + body);
}

// Liest Kopfzeilen und Inhalt einer Anfrage. false bei Abbruch oder Unsinn.
bool readRequest(int client, std::string& method, std::string& target, std::string& body) {
    std::string data;
    size_t header_end = std::string::npos;
    size_t content_length = 0;
    char buffer[4096];
    for (;;) {
        pollfd pfd{client, POLLIN, 0};
        if (poll(&pfd, 1, 2000) <= 0) return false; // Zeitueberschreitung
        const ssize_t n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;
        data.append(buffer, static_cast<size_t>(n));
        if (data.size() > golf::kMaxApiBodyBytes + 16384) return false;

        if (header_end == std::string::npos) {
            header_end = data.find("\r\n\r\n");
            if (header_end == std::string::npos) continue;
            std::istringstream first(data.substr(0, data.find("\r\n")));
            first >> method >> target;
            // Content-Length suchen (Gross-/Kleinschreibung egal).
            std::string lower = data.substr(0, header_end);
            for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const size_t pos = lower.find("\r\ncontent-length:");
            if (pos != std::string::npos) content_length = static_cast<size_t>(std::atol(lower.c_str() + pos + 17));
        }
        if (data.size() >= header_end + 4 + content_length) break;
    }
    body = data.substr(header_end + 4, content_length);
    return !method.empty() && !target.empty();
}

} // namespace

HttpServer::~HttpServer() {
    if (listener_ >= 0) close(listener_);
}

bool HttpServer::start(int port, const std::string& web_dir) {
    web_dir_ = web_dir;
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ < 0) return false;
    const int yes = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener_, 16) != 0) {
        close(listener_);
        listener_ = -1;
        return false;
    }
    fcntl(listener_, F_SETFL, fcntl(listener_, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

bool HttpServer::poll(golf::Session& session, golf::NetworkControl* network) {
    bool changed = false;
    if (listener_ < 0) return false;
    for (;;) {
        const int client = accept(listener_, nullptr, nullptr);
        if (client < 0) break;
        handleClient(client, session, network, changed);
        close(client);
    }
    return changed;
}

void HttpServer::handleClient(int client, golf::Session& session, golf::NetworkControl* network, bool& changed) {
    std::string method, target, body;
    if (!readRequest(client, method, target, body)) return;
    const std::string path = target.substr(0, target.find('?'));

    if (path.compare(0, 5, "/api/") == 0) {
        golf::ApiRequest request;
        request.method = method;
        request.path = path;
        request.body = body;
        const golf::ApiResponse response = golf::handleApi(session, request, network);
        if (method != "GET") changed = true;
        respond(client, response.status, response.content_type, response.body);
        return;
    }

    // Dateien der Web-Oberflaeche. Keine Unterordner, keine ".."-Tricks.
    if (method != "GET") {
        respond(client, 405, "text/plain", "Methode nicht erlaubt");
        return;
    }
    std::string name = path == "/" ? "index.html" : path.substr(1);
    if (name.empty() || name.find('/') != std::string::npos || name.find("..") != std::string::npos) {
        respond(client, 404, "text/plain", "Nicht gefunden");
        return;
    }
    std::ifstream file(web_dir_ + "/" + name, std::ios::binary);
    if (!file) {
        respond(client, 404, "text/plain", "Nicht gefunden");
        return;
    }
    std::stringstream content;
    content << file.rdbuf();
    respond(client, 200, contentType(name), content.str());
}

} // namespace sim
