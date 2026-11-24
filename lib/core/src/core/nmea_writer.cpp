#include "core/nmea_writer.h"

#include <cmath>
#include <cstdio>

namespace golf {

namespace {

// "4722.61400,N" bzw. "00832.50200,E": Grad und Minuten mit fuenf Nachkommastellen.
std::string coordinate(double degrees, int degree_digits, char positive, char negative) {
    const char hemisphere = degrees < 0.0 ? negative : positive;
    // In hunderttausendstel Minuten rechnen, damit beim Runden nie "60.00000" entsteht.
    const long long total = std::llround(std::fabs(degrees) * 60.0 * 100000.0);
    const long long whole_degrees = total / 6000000;
    const long long minute_part = total % 6000000;
    char text[32];
    std::snprintf(text, sizeof(text), "%0*lld%02lld.%05lld,%c", degree_digits, whole_degrees, minute_part / 100000,
                  minute_part % 100000, hemisphere);
    return text;
}

std::string timeField(const NmeaFixInfo& info) {
    if (!info.has_time) return "";
    char text[16];
    std::snprintf(text, sizeof(text), "%02d%02d%02d.000", info.hour % 24, info.minute % 60, info.second % 60);
    return text;
}

} // namespace

std::string nmeaSentence(const std::string& body) {
    unsigned char checksum = 0;
    for (char c : body) checksum ^= static_cast<unsigned char>(c);
    char tail[8];
    std::snprintf(tail, sizeof(tail), "*%02X\r\n", checksum);
    return "$" + body + tail;
}

std::string nmeaGga(const NmeaFixInfo& info) {
    if (!info.fix) return nmeaSentence("GNGGA," + timeField(info) + ",,,,,0,00,99.99,,,,,,");
    char tail[64];
    std::snprintf(tail, sizeof(tail), ",1,%02d,%.2f,%.1f,M,47.3,M,,", info.satellites, info.hdop, info.altitude_m);
    return nmeaSentence("GNGGA," + timeField(info) + "," + coordinate(info.position.lat, 2, 'N', 'S') + "," +
                        coordinate(info.position.lon, 3, 'E', 'W') + tail);
}

std::string nmeaRmc(const NmeaFixInfo& info) {
    char date[16] = "";
    if (info.has_time) std::snprintf(date, sizeof(date), "%02d%02d%02d", info.day % 100, info.month % 100, info.year % 100);
    if (!info.fix) return nmeaSentence("GNRMC," + timeField(info) + ",V,,,,,,," + date + ",,,N,V");
    char speed[24];
    std::snprintf(speed, sizeof(speed), ",%.2f,0.00,", info.speed_kmh / 1.852);
    return nmeaSentence("GNRMC," + timeField(info) + ",A," + coordinate(info.position.lat, 2, 'N', 'S') + "," +
                        coordinate(info.position.lon, 3, 'E', 'W') + speed + date + ",,,A,V");
}

std::string nmeaBurst(const NmeaFixInfo& info) {
    std::string burst = nmeaGga(info);
    burst += nmeaSentence(info.fix ? "GNGSA,A,3,04,05,09,12,17,19,22,25,,,,,1.52,0.90,1.22,1" : "GNGSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99,1");
    burst += nmeaSentence("GPGSV,3,1,11,04,67,221,42,05,23,098,35,09,81,310,44,12,15,180,29,1");
    burst += nmeaRmc(info);
    return burst;
}

} // namespace golf
