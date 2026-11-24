#include "core/lc76g.h"

namespace golf {

namespace {
const uint8_t kAskLength[] = {0x08, 0x00, 0x51, 0xAA, 0x04, 0x00, 0x00, 0x00};
const uint8_t kRequestPrefix[] = {0x00, 0x20, 0x51, 0xAA};
} // namespace

void Lc76gReader::waitFor(uint32_t now_ms, uint32_t delay_ms, State next) {
    wait_from_ = now_ms;
    wait_ms_ = delay_ms;
    state_ = next;
}

void Lc76gReader::fail(uint32_t now_ms) {
    errors_++;
    waitFor(now_ms, kErrorDelayMs, State::AskLength);
}

bool Lc76gReader::poll(uint32_t now_ms, std::string& out) {
    if (started_ && now_ms - wait_from_ < wait_ms_) return false; // noch nicht faellig
    started_ = true;

    switch (state_) {
        case State::AskLength:
            if (!bus_.write(kWriteAddress, kAskLength, sizeof(kAskLength))) {
                fail(now_ms);
                return false;
            }
            waitFor(now_ms, kStepDelayMs, State::ReadLength);
            return false;

        case State::ReadLength:
            if (!bus_.read(kReadAddress, length_bytes_, sizeof(length_bytes_))) {
                fail(now_ms);
                return false;
            }
            length_ = static_cast<uint32_t>(length_bytes_[0]) | (static_cast<uint32_t>(length_bytes_[1]) << 8) |
                      (static_cast<uint32_t>(length_bytes_[2]) << 16) | (static_cast<uint32_t>(length_bytes_[3]) << 24);
            if (length_ == 0) { // nichts Neues
                waitFor(now_ms, kIdleDelayMs, State::AskLength);
                return false;
            }
            if (length_ > kMaxChunkBytes) { // Unsinn vom Bus
                fail(now_ms);
                return false;
            }
            waitFor(now_ms, kStepDelayMs, State::RequestData);
            return false;

        case State::RequestData: {
            uint8_t request[sizeof(kRequestPrefix) + sizeof(length_bytes_)];
            for (size_t i = 0; i < sizeof(kRequestPrefix); i++) request[i] = kRequestPrefix[i];
            for (size_t i = 0; i < sizeof(length_bytes_); i++) request[sizeof(kRequestPrefix) + i] = length_bytes_[i];
            if (!bus_.write(kWriteAddress, request, sizeof(request))) {
                fail(now_ms);
                return false;
            }
            waitFor(now_ms, kStepDelayMs, State::ReadData);
            return false;
        }

        case State::ReadData: {
            std::string data(length_, '\0');
            if (!bus_.read(kReadAddress, reinterpret_cast<uint8_t*>(&data[0]), length_)) {
                fail(now_ms);
                return false;
            }
            chunks_++;
            out.swap(data);
            waitFor(now_ms, kIdleDelayMs, State::AskLength);
            return true;
        }
    }
    return false;
}

} // namespace golf
