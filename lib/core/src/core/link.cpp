#include "core/link.h"

#include <cstdio>

#include "core/crc32.h"

namespace golf {

namespace {

void putU16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>(v >> 8));
}

void putU32(std::string& out, uint32_t v) {
    for (int i = 0; i < 4; i++) out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

uint32_t getU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// Liest eine Dezimalzahl mit hoechstens `max_digits` Ziffern aus [begin, end).
bool parseNumber(const std::string& s, size_t begin, size_t end, int max_digits, uint32_t& out) {
    if (end <= begin || end - begin > static_cast<size_t>(max_digits)) return false;
    uint32_t value = 0;
    for (size_t i = begin; i < end; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        value = value * 10 + static_cast<uint32_t>(s[i] - '0');
    }
    out = value;
    return true;
}

} // namespace

// ---------- Zerlegen ----------

void LinkFragmenter::start(const std::string& message) {
    message_ = message;
    offset_ = 0;
    sequence_ = 0;
    active_ = true;
}

void LinkFragmenter::reset() {
    message_.clear();
    offset_ = 0;
    sequence_ = 0;
    active_ = false;
}

bool LinkFragmenter::next(size_t max_size, std::string& fragment) {
    if (!active_) return false;
    if (max_size < kLinkMinFragment) max_size = kLinkMinFragment;

    fragment.clear();
    const bool first = sequence_ == 0;
    fragment.push_back(static_cast<char>(first ? kLinkFirst : kLinkNext));
    putU16(fragment, sequence_);
    if (first) {
        putU32(fragment, static_cast<uint32_t>(message_.size()));
        putU32(fragment, crc32(message_.data(), message_.size()));
    }
    const size_t room = max_size - fragment.size();
    const size_t remaining = message_.size() - offset_;
    const size_t take = remaining < room ? remaining : room;
    fragment.append(message_, offset_, take);
    offset_ += take;
    sequence_++;
    if (offset_ >= message_.size()) reset(); // fertig
    return true;
}

// ---------- Zusammensetzen ----------

void LinkReassembler::reset() {
    message_.clear();
    total_ = 0;
    expected_sequence_ = 0;
    receiving_ = false;
}

LinkReassembler::Result LinkReassembler::fail() {
    errors_++;
    reset();
    return Result::Error;
}

LinkReassembler::Result LinkReassembler::add(const uint8_t* data, size_t length) {
    if (data == nullptr || length < kLinkNextHeader) return fail();
    const uint8_t type = data[0];
    const uint16_t sequence = static_cast<uint16_t>(data[1] | (data[2] << 8));
    size_t header = kLinkNextHeader;

    if (type == kLinkFirst) {
        // Ein erstes Stueck beginnt immer neu, auch mitten in einer anderen Nachricht.
        reset();
        if (length < kLinkFirstHeader || sequence != 0) return fail();
        total_ = getU32(data + 3);
        crc_ = getU32(data + 7);
        if (total_ > kLinkMaxMessage) return fail();
        message_.reserve(total_);
        receiving_ = true;
        header = kLinkFirstHeader;
    } else if (type == kLinkNext) {
        if (!receiving_ || sequence != expected_sequence_) return fail();
    } else {
        return fail();
    }

    const size_t payload = length - header;
    if (message_.size() + payload > total_) return fail(); // mehr Daten als angekuendigt
    // Ein weiteres Stueck ohne Inhalt bringt nichts voran und waere eine Endlosschleife wert.
    if (type == kLinkNext && payload == 0) return fail();
    message_.append(reinterpret_cast<const char*>(data + header), payload);
    expected_sequence_ = static_cast<uint16_t>(sequence + 1);

    if (message_.size() < total_) return Result::Incomplete;
    receiving_ = false;
    if (crc32(message_.data(), message_.size()) != crc_) return fail();
    return Result::Complete;
}

// ---------- Anfragen und Antworten ----------

std::string encodeLinkRequest(uint32_t id, const ApiRequest& request) {
    return std::to_string(id) + "\n" + request.method + " " + request.path + "\n" + request.body;
}

bool decodeLinkRequest(const std::string& message, uint32_t& id, ApiRequest& request) {
    const size_t first = message.find('\n');
    if (first == std::string::npos) return false;
    const size_t second = message.find('\n', first + 1);
    if (second == std::string::npos) return false;
    uint32_t number = 0;
    if (!parseNumber(message, 0, first, 9, number)) return false;

    const std::string line = message.substr(first + 1, second - first - 1);
    const size_t space = line.find(' ');
    if (space == std::string::npos || space == 0 || space > 8) return false;
    for (size_t i = 0; i < space; i++) {
        if (line[i] < 'A' || line[i] > 'Z') return false;
    }
    const std::string path = line.substr(space + 1);
    if (path.empty() || path[0] != '/' || path.size() > 128) return false;
    for (char c : path) {
        if (static_cast<unsigned char>(c) <= 0x20 || c == 0x7F) return false; // keine Leer- oder Steuerzeichen
    }

    id = number;
    request.method = line.substr(0, space);
    request.path = path;
    request.body = message.substr(second + 1);
    return true;
}

std::string encodeLinkResponse(uint32_t id, const ApiResponse& response) {
    return std::to_string(id) + "\n" + std::to_string(response.status) + "\n" + response.body;
}

bool decodeLinkResponse(const std::string& message, uint32_t& id, int& status, std::string& body) {
    const size_t first = message.find('\n');
    if (first == std::string::npos) return false;
    const size_t second = message.find('\n', first + 1);
    if (second == std::string::npos) return false;
    uint32_t number = 0, code = 0;
    if (!parseNumber(message, 0, first, 9, number)) return false;
    if (!parseNumber(message, first + 1, second, 3, code) || code < 100) return false;
    id = number;
    status = static_cast<int>(code);
    body = message.substr(second + 1);
    return true;
}

// ---------- Gegenstelle ----------

void LinkServer::respondError(uint32_t id, int status, const char* message) {
    ApiResponse response;
    response.status = status;
    response.body = std::string("{\"error\":\"") + message + "\"}";
    outgoing_.start(encodeLinkResponse(id, response));
}

void LinkServer::disconnect() {
    incoming_.reset();
    outgoing_.reset();
    error_reported_ = false;
}

bool LinkServer::receive(const uint8_t* data, size_t length) {
    const bool is_first = data != nullptr && length > 0 && data[0] == kLinkFirst;
    if (is_first) {
        // Eine neue Anfrage: Eine noch nicht abgeholte Antwort ist ueberholt.
        outgoing_.reset();
        error_reported_ = false;
    }

    const LinkReassembler::Result result = incoming_.add(data, length);
    if (result == LinkReassembler::Result::Incomplete) return false;
    if (result == LinkReassembler::Result::Error) {
        // Nur einmal pro verunglueckter Nachricht melden, nicht fuer jedes Folgestueck.
        if (!error_reported_) {
            error_reported_ = true;
            respondError(0, 400, "Uebertragungsfehler");
        }
        return false;
    }

    uint32_t id = 0;
    ApiRequest request;
    if (!decodeLinkRequest(incoming_.message(), id, request)) {
        respondError(0, 400, "Ungueltige Anfrage");
        return false;
    }
    outgoing_.start(encodeLinkResponse(id, handleApi(session_, request, network_)));
    handled_++;
    return true;
}

} // namespace golf
