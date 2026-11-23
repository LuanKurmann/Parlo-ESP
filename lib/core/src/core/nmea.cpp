#include "core/nmea.h"

#include <cstring>

namespace golf {

namespace {

constexpr int kMaxFields = 24;
constexpr double kKnotsToKmh = 1.852;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Liest eine Dezimalzahl wie "12", "0.9" oder "-46.9". Nichts anderes ist erlaubt.
bool parseDecimal(const char* s, double& out) {
    bool negative = false;
    if (*s == '-') {
        negative = true;
        s++;
    }
    if (!isDigit(*s)) return false;
    double value = 0.0;
    int digits = 0;
    while (isDigit(*s)) {
        value = value * 10.0 + (*s - '0');
        s++;
        if (++digits > 9) return false;
    }
    if (*s == '.') {
        s++;
        double scale = 0.1;
        while (isDigit(*s)) {
            value += (*s - '0') * scale;
            scale *= 0.1;
            s++;
        }
    }
    if (*s != '\0') return false;
    out = negative ? -value : value;
    return true;
}

bool parseInt(const char* s, int& out) {
    if (!isDigit(*s)) return false;
    int value = 0;
    int digits = 0;
    while (isDigit(*s)) {
        value = value * 10 + (*s - '0');
        s++;
        if (++digits > 6) return false;
    }
    if (*s != '\0') return false;
    out = value;
    return true;
}

// Liest "ddmm.mmmm" (Breite, degree_digits = 2) oder "dddmm.mmmm" (Laenge, 3).
bool parseCoordinate(const char* s, const char* hemisphere, int degree_digits, char positive, char negative,
                     double max_degrees, double& out) {
    for (int i = 0; i < degree_digits + 2; i++) {
        if (!isDigit(s[i])) return false;
    }
    if (s[degree_digits + 2] != '.' && s[degree_digits + 2] != '\0') return false;

    int degrees = 0;
    for (int i = 0; i < degree_digits; i++) degrees = degrees * 10 + (s[i] - '0');
    double minutes = 0.0;
    if (!parseDecimal(s + degree_digits, minutes) || minutes >= 60.0) return false;

    const double value = degrees + minutes / 60.0;
    if (value > max_degrees) return false;

    if (hemisphere[0] == '\0' || hemisphere[1] != '\0') return false;
    if (hemisphere[0] == positive) {
        out = value;
    } else if (hemisphere[0] == negative) {
        out = -value;
    } else {
        return false;
    }
    return true;
}

bool parsePosition(const char* lat, const char* ns, const char* lon, const char* ew, LatLon& out) {
    LatLon p{0.0, 0.0};
    if (!parseCoordinate(lat, ns, 2, 'N', 'S', 90.0, p.lat)) return false;
    if (!parseCoordinate(lon, ew, 3, 'E', 'W', 180.0, p.lon)) return false;
    out = p;
    return true;
}

int twoDigits(const char* s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

// Liest "hhmmss" oder "hhmmss.sss".
void parseTime(const char* s, GpsFix& fix) {
    for (int i = 0; i < 6; i++) {
        if (!isDigit(s[i])) return;
    }
    if (s[6] != '\0' && s[6] != '.') return;
    const int h = twoDigits(s), m = twoDigits(s + 2), sec = twoDigits(s + 4);
    if (h > 23 || m > 59 || sec > 60) return;
    fix.hour = h;
    fix.minute = m;
    fix.second = sec;
    fix.has_time = true;
}

// Liest "ddmmyy".
void parseDate(const char* s, GpsFix& fix) {
    for (int i = 0; i < 6; i++) {
        if (!isDigit(s[i])) return;
    }
    if (s[6] != '\0') return;
    const int d = twoDigits(s), m = twoDigits(s + 2), y = twoDigits(s + 4);
    if (d < 1 || d > 31 || m < 1 || m > 12) return;
    fix.day = d;
    fix.month = m;
    fix.year = 2000 + y;
    fix.has_date = true;
}

} // namespace

bool NmeaParser::feed(const char* data, size_t length) {
    bool changed = false;
    for (size_t i = 0; i < length; i++) {
        if (feed(data[i])) changed = true;
    }
    return changed;
}

bool NmeaParser::feed(char c) {
    if (c == '$') {
        in_sentence_ = true;
        length_ = 0;
        return false;
    }
    if (!in_sentence_) return false;

    if (c == '\r' || c == '\n') {
        in_sentence_ = false;
        buffer_[length_] = '\0';
        const bool changed = processSentence();
        length_ = 0;
        return changed;
    }
    if (length_ >= kMaxLength) {
        in_sentence_ = false;
        length_ = 0;
        rejected_++;
        return false;
    }
    buffer_[length_++] = c;
    return false;
}

bool NmeaParser::processSentence() {
    // Aufbau: <Inhalt>*<zwei Hex-Ziffern>
    if (length_ < 9 || buffer_[length_ - 3] != '*') {
        rejected_++;
        return false;
    }
    const int high = hexValue(buffer_[length_ - 2]);
    const int low = hexValue(buffer_[length_ - 1]);
    if (high < 0 || low < 0) {
        rejected_++;
        return false;
    }
    uint8_t checksum = 0;
    for (size_t i = 0; i < length_ - 3; i++) checksum ^= static_cast<uint8_t>(buffer_[i]);
    if (checksum != static_cast<uint8_t>(high * 16 + low)) {
        rejected_++;
        return false;
    }
    buffer_[length_ - 3] = '\0';

    char* fields[kMaxFields];
    int count = 0;
    fields[count++] = buffer_;
    for (char* p = buffer_; *p != '\0'; p++) {
        if (*p == ',') {
            *p = '\0';
            if (count == kMaxFields) {
                rejected_++;
                return false;
            }
            fields[count++] = p + 1;
        }
    }

    // Satzname: zwei Zeichen Satellitensystem, drei Zeichen Typ, z. B. "GNGGA".
    if (std::strlen(fields[0]) != 5) {
        rejected_++;
        return false;
    }
    accepted_++;
    const char* type = fields[0] + 2;
    if (std::strcmp(type, "GGA") == 0) return handleGga(fields, count);
    if (std::strcmp(type, "RMC") == 0) return handleRmc(fields, count);
    return false;
}

// GGA: 1 Zeit, 2 Breite, 3 N/S, 4 Laenge, 5 E/W, 6 Qualitaet, 7 Satelliten, 8 HDOP, 9 Hoehe
bool NmeaParser::handleGga(char** f, int count) {
    int quality = 0;
    if (count < 10 || !parseInt(f[6], quality)) return false;

    parseTime(f[1], fix_);
    int satellites = 0;
    if (parseInt(f[7], satellites)) fix_.satellites = satellites;
    double value = 0.0;
    if (parseDecimal(f[8], value)) fix_.hdop = static_cast<float>(value);

    if (quality == 0 || !parsePosition(f[2], f[3], f[4], f[5], fix_.position)) {
        fix_.valid = false;
        return true;
    }
    if (parseDecimal(f[9], value)) fix_.altitude_m = static_cast<float>(value);
    fix_.valid = true;
    return true;
}

// RMC: 1 Zeit, 2 Status, 3 Breite, 4 N/S, 5 Laenge, 6 E/W, 7 Knoten, 8 Kurs, 9 Datum
bool NmeaParser::handleRmc(char** f, int count) {
    if (count < 10 || (std::strcmp(f[2], "A") != 0 && std::strcmp(f[2], "V") != 0)) return false;

    parseTime(f[1], fix_);
    parseDate(f[9], fix_);

    if (f[2][0] == 'V' || !parsePosition(f[3], f[4], f[5], f[6], fix_.position)) {
        fix_.valid = false;
        return true;
    }
    double knots = 0.0;
    if (parseDecimal(f[7], knots)) fix_.speed_kmh = static_cast<float>(knots * kKnotsToKmh);
    fix_.valid = true;
    return true;
}

} // namespace golf
