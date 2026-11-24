#include "screenshot.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include <lvgl.h>

namespace sim {

namespace {
void put32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}
} // namespace

bool saveScreenshot(const std::string& path, bool round_mask) {
    lv_refr_now(nullptr);
    lv_draw_buf_t* shot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_XRGB8888);
    if (shot == nullptr) return false;

    const int w = static_cast<int>(shot->header.w);
    const int h = static_cast<int>(shot->header.h);
    const int row_bytes = (w * 3 + 3) & ~3;

    std::vector<uint8_t> out;
    out.reserve(54 + static_cast<size_t>(row_bytes) * static_cast<size_t>(h));
    out.push_back('B');
    out.push_back('M');
    put32(out, 54 + static_cast<uint32_t>(row_bytes * h));
    put32(out, 0);
    put32(out, 54);
    put32(out, 40);
    put32(out, static_cast<uint32_t>(w));
    put32(out, static_cast<uint32_t>(h));
    put16(out, 1);
    put16(out, 24);
    put32(out, 0);
    put32(out, static_cast<uint32_t>(row_bytes * h));
    put32(out, 2835);
    put32(out, 2835);
    put32(out, 0);
    put32(out, 0);

    const double radius = w / 2.0;
    for (int y = h - 1; y >= 0; y--) { // BMP speichert von unten nach oben
        const uint8_t* row = shot->data + static_cast<size_t>(y) * shot->header.stride;
        for (int x = 0; x < w; x++) {
            const double dx = x + 0.5 - radius, dy = y + 0.5 - radius;
            if (round_mask && dx * dx + dy * dy > radius * radius) {
                out.push_back(0x2A); // Gehaeusefarbe #23282A als B, G, R
                out.push_back(0x28);
                out.push_back(0x23);
            } else {
                out.push_back(row[x * 4 + 0]);
                out.push_back(row[x * 4 + 1]);
                out.push_back(row[x * 4 + 2]);
            }
        }
        for (int pad = w * 3; pad < row_bytes; pad++) out.push_back(0);
    }
    lv_draw_buf_destroy(shot);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), file) == out.size();
    return std::fclose(file) == 0 && ok;
}

} // namespace sim
