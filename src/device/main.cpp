// Firmware-Einstieg fuer das Geraet: verbindet die Hardware mit dem Kern.
//
// Stand: kompiliert, aber noch nie auf dem Geraet gelaufen. Die Reihenfolge
// der Inbetriebnahme steht in docs/inbetriebnahme.md.

#include <Arduino.h>
#include <esp_random.h>
#include <Wire.h>
#include <lvgl.h>

#include "battery.h"
#include "board.h"
#include "core/app_info.h"
#include "core/button.h"
#include "core/gps_feed.h"
#include "core/lc76g.h"
#include "core/power.h"
#include "core/session.h"
#include "display.h"
#include "net.h"
#include "storage.h"
#include "ui/ui.h"
#include "wire_bus.h"

namespace {

constexpr uint32_t kUiRefreshMs = 100;
constexpr uint32_t kPwrPollMs = 20;
constexpr uint32_t kBatteryPollMs = 10000;

device::FlashFileSystem storage;
device::Net net;
WireBus i2c;
golf::Lc76gReader gps_reader(i2c);
golf::Button button1, button2;
golf::ScreenPower screen_power;
uint8_t shown_brightness = 255;
uint32_t last_battery_poll_ms = 0;
bool battery_ok = false;

// Gross; deshalb auf dem Heap statt auf dem Stack der Hauptschleife.
golf::Session* session = nullptr;
golf::GpsFeed* gps_feed = nullptr;

bool pwr_pressed = false;
uint32_t last_pwr_poll_ms = 0;
uint32_t last_ui_refresh_ms = 0;
uint32_t last_input_count = 0;
std::string gps_chunk;

uint32_t tickMs() { return millis(); }

bool touchGate(bool pressed) { return screen_power.onTouch(pressed, millis()); }

// Der PWR-Knopf haengt am I/O-Baustein: Bit 4 des Eingangsregisters, 1 = gedrueckt.
bool readPwrButton() {
    Wire.beginTransmission(board::kExpanderAddress);
    Wire.write(board::kExpanderInputRegister);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(board::kExpanderAddress, static_cast<uint8_t>(1)) != 1) return false;
    return (Wire.read() >> board::kExpanderPwrButtonBit) & 1;
}

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.printf("Golf-Tracker %s\n", golf::version());

    pinMode(board::kBootButton, INPUT_PULLUP);
    Wire.begin(board::kI2cSda, board::kI2cScl, board::kI2cHz);

    lv_init();
    lv_tick_set_cb(tickMs);
    const bool display_ok = device::displayBegin();
    const bool touch_ok = display_ok && device::touchBegin();
    const bool storage_ok = storage.begin();
    battery_ok = device::batteryBegin();
    device::touchSetGate(touchGate);
    Serial.printf("Display %s, Touch %s, Speicher %s, Akku-Chip %s\n", display_ok ? "ok" : "FEHLT",
                  touch_ok ? "ok" : "FEHLT", storage_ok ? "ok" : "FEHLT", battery_ok ? "ok" : "FEHLT");

    session = new golf::Session(storage, esp_random); // Hardware-Zufall fuer neue Kennungen
    gps_feed = new golf::GpsFeed(*session);
    net.begin();
    if (display_ok) golf::ui::init(session->app());
}

void loop() {
    const uint32_t now = millis();
    golf::App& app = session->app();

    // Knopf 1 = BOOT (direkt am Prozessor), Knopf 2 = PWR (ueber den I/O-Baustein).
    if (now - last_pwr_poll_ms >= kPwrPollMs) {
        last_pwr_poll_ms = now;
        pwr_pressed = readPwrButton();
    }
    // Ist das Display aus, weckt ein Knopf es nur.
    golf::dispatchButton(app, screen_power, 1, button1.update(digitalRead(board::kBootButton) == LOW, now), now);
    golf::dispatchButton(app, screen_power, 2, button2.update(pwr_pressed, now), now);

    screen_power.tick(now, app.phoneWanted());
    if (screen_power.brightness() != shown_brightness) {
        shown_brightness = screen_power.brightness();
        device::displaySetBrightness(shown_brightness);
    }

    if (battery_ok && now - last_battery_poll_ms >= kBatteryPollMs) {
        last_battery_poll_ms = now;
        app.setBattery(device::batteryRead());
    }

    if (gps_reader.poll(now, gps_chunk)) gps_feed->feed(gps_chunk.data(), gps_chunk.size(), now);
    gps_feed->tick(now);

    net.loop(*session, app.phoneWanted(), now);
    app.setNetStatus(net.status());

    session->sync();

    // Sofort nach einer Eingabe neu zeichnen, sonst zehnmal pro Sekunde (neue GPS-Position).
    if (app.inputCount() != last_input_count || now - last_ui_refresh_ms >= kUiRefreshMs) {
        last_input_count = app.inputCount();
        last_ui_refresh_ms = now;
        golf::ui::refresh();
    }
    lv_timer_handler();
    delay(5);
}
