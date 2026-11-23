// Firmware-Einstieg fuer das Geraet.
//
// Stand Schritt 1: Die Oberflaeche wird in einen Platzhalter-Puffer gezeichnet,
// der noch nirgends angezeigt wird. Der echte AMOLED-Treiber folgt, sobald das
// Board da ist (Schritt 11/12).

#include <Arduino.h>
#include <lvgl.h>

#include "core/app_info.h"
#include "ui/ui.h"

namespace {

constexpr int kBufferLines = 40;
uint8_t* draw_buffer = nullptr;

void flush_placeholder(lv_display_t* display, const lv_area_t*, uint8_t*) {
    lv_display_flush_ready(display);
}

uint32_t tick_ms() { return millis(); }

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.printf("Golf-Tracker %s\n", golf::version());

    lv_init();
    lv_tick_set_cb(tick_ms);

    const size_t buffer_bytes = golf::kDisplaySize * kBufferLines * 2; // RGB565
    draw_buffer = static_cast<uint8_t*>(ps_malloc(buffer_bytes));
    lv_display_t* display = lv_display_create(golf::kDisplaySize, golf::kDisplaySize);
    lv_display_set_flush_cb(display, flush_placeholder);
    lv_display_set_buffers(display, draw_buffer, nullptr, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    golf::ui::init();
}

void loop() {
    lv_timer_handler();
    delay(5);
}
