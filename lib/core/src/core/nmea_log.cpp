#include "core/nmea_log.h"

namespace golf {

namespace {

// Zeitfeld eines GGA- oder RMC-Satzes ("081530.000"), sonst leer.
std::string timeOf(const std::string& line) {
    if (line.size() < 8) return "";
    const std::string type = line.substr(3, 3);
    if (type != "GGA" && type != "RMC") return "";
    const size_t start = line.find(',');
    if (start == std::string::npos) return "";
    const size_t end = line.find(',', start + 1);
    if (end == std::string::npos) return "";
    return line.substr(start + 1, end - start - 1);
}

} // namespace

NmeaLog::NmeaLog(const std::string& text) {
    std::string current;
    std::string current_time;
    bool has_time = false;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] != '$') continue;

        // Ein neuer Messzeitpunkt beginnt, sobald ein GGA/RMC eine andere Zeit traegt.
        const std::string time = timeOf(line);
        const bool is_position_sentence = line.size() >= 6 && (line.compare(3, 3, "GGA") == 0 || line.compare(3, 3, "RMC") == 0);
        if (is_position_sentence) {
            if (has_time && time != current_time && !current.empty()) {
                bursts_.push_back(current);
                current.clear();
            }
            current_time = time;
            has_time = true;
        }
        current += line;
        current += "\r\n";
    }
    if (!current.empty()) bursts_.push_back(current);
}

bool NmeaLog::next(std::string& burst) {
    if (next_ >= bursts_.size()) return false;
    burst = bursts_[next_++];
    return true;
}

} // namespace golf
