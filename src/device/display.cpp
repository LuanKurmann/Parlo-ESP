#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TouchDrvCSTXXX.hpp>
#include <Wire.h>
#include <lvgl.h>

#include "board.h"
#include "core/app_info.h"

namespace device {

namespace {

constexpr int kBufferLines = 60; // Zeilen pro Zeichendurchgang

Arduino_DataBus* bus = nullptr;
Arduino_CO5300* panel = nullptr;
TouchDrvCST92xx touch;
bool touch_ok = false;
bool (*touch_gate)(bool pressed) = nullptr;

void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;
    panel->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(pixels), width, height);
    lv_display_flush_ready(display);
}

// Der CO5300 nimmt nur Bereiche an, die auf geraden Koordinaten beginnen und
// eine gerade Breite und Hoehe haben.
void roundArea(lv_event_t* event) {
    lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    int16_t x[1] = {0}, y[1] = {0};
    bool pressed = touch_ok && touch.getPoint(x, y, 1) > 0;
    if (touch_gate != nullptr) pressed = touch_gate(pressed);
    if (pressed) {
        data->point.x = x[0];
        data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

} // namespace

bool displayBegin() {
    bus = new Arduino_ESP32QSPI(board::kLcdCs, board::kLcdSclk, board::kLcdSdio0, board::kLcdSdio1, board::kLcdSdio2,
                                board::kLcdSdio3);
    panel = new Arduino_CO5300(bus, board::kLcdReset, 0 /* Drehung */, golf::kDisplaySize, golf::kDisplaySize,
                               board::kLcdColumnOffset, 0, 0, 0);
    if (!panel->begin()) return false;
    panel->fillScreen(0x0000);
    panel->setBrightness(200);

    const size_t buffer_bytes = static_cast<size_t>(golf::kDisplaySize) * kBufferLines * 2; // RGB565
    uint8_t* buffer = static_cast<uint8_t*>(ps_malloc(buffer_bytes));
    if (buffer == nullptr) buffer = static_cast<uint8_t*>(malloc(buffer_bytes));
    if (buffer == nullptr) return false;

    lv_display_t* display = lv_display_create(golf::kDisplaySize, golf::kDisplaySize);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, buffer, nullptr, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_add_event_cb(display, roundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
    return true;
}

void touchSetGate(bool (*gate)(bool pressed)) { touch_gate = gate; }

void displaySetBrightness(uint8_t value) {
    if (panel != nullptr) panel->setBrightness(value);
}

bool touchBegin() {
    touch.setPins(board::kTouchReset, board::kTouchInt);
    touch_ok = touch.begin(Wire, board::kTouchAddress, board::kI2cSda, board::kI2cScl);
    // Das Eingabegeraet wird auch ohne Touch angemeldet; es meldet dann nie eine Beruehrung.
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);
    return touch_ok;
}

} // namespace device
