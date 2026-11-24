// PC-Simulator: zeigt die Oberflaeche in einem SDL-Fenster (466 x 466) und
// ersetzt Knoepfe und GPS durch die Tastatur.
//
// Tasten im Fenster:
//   1, 2            Knopf 1 und Knopf 2 (kurz tippen oder lang halten)
//   Maus            Touch: tippen und wischen
//   Pfeil links/rechts  Ansicht wechseln (wie Wischen)
//   Pfeil hoch/ab   auf der Bahn Richtung Gruen bzw. zurueck gehen
//   G               GPS-Empfang aus/ein
//   S               Screenshot nach screenshot.bmp
//
// Aufruf:
//   program [--data <ordner>] [--headless] [--do <befehle>]
//           [--replay <datei.nmea>] [--speed <faktor>] [--demo-track <datei.nmea>]
//           [--serve <port>] [--web <ordner>]
//
//   --data      Ordner fuer Platz und Runden (Standard: sim_data)
//   --no-course beginnt ohne Platzdaten statt mit dem Beispielplatz
//   --replay    spielt eine GPS-Aufzeichnung ab statt der Pfeiltasten. Die
//               Daten laufen durch denselben Weg wie auf dem Geraet.
//   --speed     Zeitraffer der Wiedergabe im Fenster (Standard: 10)
//   --serve     startet die Handy-Verbindung als lokalen Webserver: Die
//               Web-Oberflaeche ist dann unter http://127.0.0.1:<port>/ erreichbar.
//               Mit --headless laeuft der Simulator dabei weiter, bis er beendet wird.
//   --web       Ordner der Web-Oberflaeche (Standard: web)
//   --demo-track  schreibt eine Beispielaufzeichnung (drei Loecher des
//               Beispielplatzes) in die Datei und beendet sich
//   --headless  ohne Fenster; beendet sich nach den Befehlen. Ohne --data
//               beginnt jeder Lauf mit leerem Speicher.
//   --do        Befehle, mit Komma getrennt, werden beim Start ausgefuehrt:
//                 b1 b2        Knopf kurz        B1 B2   Knopf lang
//                 pos:218      218 m vom Abschlag entfernt stehen
//                 gps:off / gps:on
//                 click:233:300    Display an der Stelle x:y antippen
//                 swipe:left / swipe:right   ueber das Display wischen
//                 wait:2000        2 Sekunden warten (z. B. bis eine Einblendung weg ist)
//                 next / prev      naechste bzw. vorherige Ansicht (ohne Touch)
//                 shot:datei.bmp   Screenshot speichern
//                 rawshot:datei.bmp  Screenshot ohne runde Maske (fuer Tests)
//                 net:hotspot / net:connecting / net:connected / net:off
//                                  Zustand des simulierten WLANs setzen
//                 bat:87 / bat:charging:50 / bat:none   Akkustand setzen
//                 bag:Dr+H3+E7+SW   Schlaeger im Bag festlegen (mit + getrennt)
//                 play:60          mit --replay: 60 Sekunden der Aufzeichnung abspielen
//
// Beispiel: program --headless --do "b1,b1,pos:218,shot:distanz.bmp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <unistd.h>

#include "core/app_info.h"
#include "core/button.h"
#include "core/posix_fs.h"
#include "core/power.h"
#include "core/session.h"
#include "core/demo_course.h"
#include "core/gps_feed.h"
#include "core/nmea_log.h"
#include "demo_track.h"
#include "http_server.h"
#include "screenshot.h"
#include "ui/ui.h"

namespace {

// Zufallszahlen fuer neue Kennungen.
uint32_t simulatorRandom() {
    static std::random_device device;
    return device();
}

// Simuliertes WLAN: merkt sich nur, was das Handy einstellt.
class FakeNetwork : public golf::NetworkControl {
public:
    golf::NetStatus current;
    std::string saved_ssid;

    void set(golf::NetMode mode, const char* ssid, const char* address) {
        current.mode = mode;
        std::snprintf(current.ssid, sizeof(current.ssid), "%s", ssid);
        std::snprintf(current.address, sizeof(current.address), "%s", address);
        std::snprintf(current.hotspot_password, sizeof(current.hotspot_password), "%s",
                      mode == golf::NetMode::Hotspot ? "48213975" : "");
    }
    golf::NetStatus status() const override { return current; }
    bool saveNetwork(const char* ssid, const char*) override {
        saved_ssid = ssid;
        current.has_saved_network = true;
        set(golf::NetMode::Connected, ssid, current.address);
        return true;
    }
    bool forgetNetwork() override {
        saved_ssid.clear();
        current.has_saved_network = false;
        set(golf::NetMode::Hotspot, "Golf-Tracker", current.address);
        return true;
    }
};

// Laesst LVGL `ms` Millisekunden lang arbeiten (Eingaben, Timer, Zeichnen).
void pump(uint32_t ms) {
    const uint32_t end = SDL_GetTicks() + ms;
    do {
        lv_timer_handler();
        SDL_Delay(5);
    } while (SDL_GetTicks() < end);
}

// Zeiger fuer Beruehrungen aus Befehlen (--do), unabhaengig von der Maus.
struct ScriptPointer {
    int x = 0, y = 0;
    bool pressed = false;

    static void read(lv_indev_t* indev, lv_indev_data_t* data) {
        const ScriptPointer* self = static_cast<const ScriptPointer*>(lv_indev_get_user_data(indev));
        data->point.x = self->x;
        data->point.y = self->y;
        data->state = self->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    }

    void attach() {
        lv_indev_t* indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_user_data(indev, this);
        lv_indev_set_read_cb(indev, read);
    }

    void click(int px, int py) {
        x = px;
        y = py;
        pressed = true;
        pump(80);
        pressed = false;
        pump(80);
    }

    void swipe(int direction) { // +1 = nach rechts, -1 = nach links
        x = 233 - direction * 100;
        y = 233;
        pressed = true;
        pump(40);
        for (int step = 0; step < 6; step++) {
            x += direction * 35;
            pump(35);
        }
        pressed = false;
        pump(80);
    }
};

struct Simulator {
    std::unique_ptr<golf::PosixFileSystem> fs;
    std::unique_ptr<golf::Session> session;
    ScriptPointer pointer;
    FakeNetwork network;
    std::unique_ptr<golf::GpsFeed> feed;   // nur bei --replay
    std::unique_ptr<golf::NmeaLog> track;
    uint32_t track_ms = 0;                 // Zeit innerhalb der Aufzeichnung
    double progress_m = 0.0; // Entfernung vom Abschlag auf der Bahn
    bool gps_on = true;
    int hole = -1;

    golf::App& app() { return session->app(); }

    // Spielt die naechsten `seconds` Sekunden der Aufzeichnung ab. Liefert
    // false, wenn sie zu Ende ist.
    bool play(int seconds) {
        std::string burst;
        for (int i = 0; i < seconds; i++) {
            if (!track->next(burst)) return false;
            track_ms += 1000;
            feed->feed(burst.data(), burst.size(), track_ms);
            feed->tick(track_ms);
        }
        return true;
    }

    // Uebertraegt die simulierte Position in die App und sichert den Stand.
    void apply() {
        app().setNetStatus(network.status());
        if (track) { // die Position kommt aus der Aufzeichnung
            session->sync();
            golf::ui::refresh();
            return;
        }
        if (app().currentHole() != hole) { // neues Loch: zurueck an den Abschlag
            hole = app().currentHole();
            progress_m = 0.0;
        }
        if (gps_on) {
            const golf::Hole& current = app().course().holes[hole];
            // Ohne Platzdaten steht der Spieler `progress_m` noerdlich eines festen Punkts.
            app().setPosition(current.has_tee && current.has_green
                                  ? golf::positionOnHole(current, progress_m)
                                  : golf::LatLon{47.3769 + progress_m / 111180.0, 8.5417});
        } else {
            app().clearPosition();
        }
        session->sync();
        golf::ui::refresh();
    }

    bool run(const std::string& command) {
        if (command == "b1") {
            app().button1Short();
        } else if (command == "B1") {
            app().button1Long();
        } else if (command == "b2") {
            app().button2Short();
        } else if (command == "B2") {
            app().button2Long();
        } else if (command == "gps:off") {
            gps_on = false;
        } else if (command == "gps:on") {
            gps_on = true;
        } else if (command.compare(0, 4, "pos:") == 0) {
            progress_m = std::atof(command.c_str() + 4);
        } else if (command.compare(0, 6, "click:") == 0) {
            int px = 0, py = 0;
            if (std::sscanf(command.c_str() + 6, "%d:%d", &px, &py) != 2) return false;
            apply();
            pointer.click(px, py);
        } else if (command == "swipe:left") {
            apply();
            pointer.swipe(-1);
        } else if (command == "swipe:right") {
            apply();
            pointer.swipe(1);
        } else if (command.compare(0, 5, "play:") == 0) {
            if (!track) return false;
            play(std::atoi(command.c_str() + 5));
        } else if (command.compare(0, 5, "wait:") == 0) {
            apply();
            pump(static_cast<uint32_t>(std::atoi(command.c_str() + 5)));
        } else if (command == "net:hotspot") {
            network.set(golf::NetMode::Hotspot, "Golf-Tracker", "192.168.4.1");
        } else if (command == "net:connecting") {
            network.set(golf::NetMode::Connecting, "Heimnetz", "");
        } else if (command == "net:connected") {
            network.set(golf::NetMode::Connected, "Heimnetz", "192.168.1.57");
        } else if (command == "net:off") {
            network.set(golf::NetMode::Off, "", "");
        } else if (command.compare(0, 4, "bag:") == 0) {
            std::string list = "{\"v\":1,\"clubs\":[";
            std::string codes = command.substr(4);
            size_t start = 0;
            for (bool first = true; start <= codes.size(); first = false) {
                size_t end = codes.find('+', start);
                if (end == std::string::npos) end = codes.size();
                list += std::string(first ? "" : ",") + "\"" + codes.substr(start, end - start) + "\"";
                start = end + 1;
            }
            golf::Bag bag = app().bag();
            if (!golf::bagFromJson(list + "]}", bag) || !session->setBag(bag)) return false;
        } else if (command.compare(0, 4, "bat:") == 0) {
            golf::BatteryInfo battery;
            const std::string value = command.substr(4);
            if (value != "none") {
                battery.known = true;
                battery.charging = value.compare(0, 9, "charging:") == 0;
                battery.percent = std::atoi(value.c_str() + (battery.charging ? 9 : 0));
            }
            app().setBattery(battery);
        } else if (command == "next") {
            app().swipe(1);
        } else if (command == "prev") {
            app().swipe(-1);
        } else if (command.compare(0, 5, "shot:") == 0) {
            apply();
            lv_timer_handler();
            return sim::saveScreenshot(command.substr(5));
        } else if (command.compare(0, 8, "rawshot:") == 0) {
            apply();
            lv_timer_handler();
            return sim::saveScreenshot(command.substr(8), false);
        } else {
            return false;
        }
        apply();
        return true;
    }

    bool runAll(const std::string& commands) {
        size_t start = 0;
        while (start <= commands.size()) {
            size_t end = commands.find(',', start);
            if (end == std::string::npos) end = commands.size();
            const std::string command = commands.substr(start, end - start);
            if (!command.empty() && !run(command)) {
                std::fprintf(stderr, "Befehl fehlgeschlagen: %s\n", command.c_str());
                return false;
            }
            start = end + 1;
        }
        return true;
    }
};

// Dunkler Ring ueber den Ecken, damit das Fenster wie das runde Display aussieht.
void addRoundBezel() {
    constexpr int kRing = 200;
    lv_obj_t* ring = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, golf::kDisplaySize + 2 * kRing, golf::kDisplaySize + 2 * kRing);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, kRing, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x23282A), 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
}

// Zeigt Dimmen und Abschalten des Displays im Fenster als dunklen Schleier.
lv_obj_t* addDimVeil() {
    lv_obj_t* veil = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(veil);
    lv_obj_set_size(veil, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(veil, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(veil, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(veil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_background(veil); // unter den Gehaeusering
    return veil;
}

// Liefert true genau in dem Durchlauf, in dem die Taste neu gedrueckt wurde.
bool pressedNow(const Uint8* keys, SDL_Scancode code, bool& was_down) {
    const bool down = keys[code] != 0;
    const bool edge = down && !was_down;
    was_down = down;
    return edge;
}

} // namespace

int main(int argc, char** argv) {
    bool headless = false;
    bool with_course = true;
    std::string data_dir;
    std::string commands;
    std::string replay_path;
    double speed = 10.0;
    int serve_port = 0;
    std::string web_dir = "web";
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--headless" || arg == "--smoke") {
            headless = true;
        } else if (arg == "--no-course") {
            with_course = false;
        } else if (arg == "--data" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--do" && i + 1 < argc) {
            commands = argv[++i];
        } else if (arg == "--replay" && i + 1 < argc) {
            replay_path = argv[++i];
        } else if (arg == "--serve" && i + 1 < argc) {
            serve_port = std::atoi(argv[++i]);
        } else if (arg == "--web" && i + 1 < argc) {
            web_dir = argv[++i];
        } else if (arg == "--speed" && i + 1 < argc) {
            speed = std::atof(argv[++i]);
            if (speed < 0.1) speed = 0.1;
        } else if (arg == "--demo-track" && i + 1 < argc) {
            std::ofstream out(argv[++i], std::ios::binary);
            out << sim::demoTrack();
            return out.good() ? 0 : 1;
        } else {
            std::fprintf(stderr, "Unbekanntes Argument: %s\n", arg.c_str());
            return 2;
        }
    }
    if (headless) setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (data_dir.empty()) {
        if (headless) {
            char pattern[] = "/tmp/golf_sim_XXXXXX";
            const char* dir = mkdtemp(pattern);
            if (dir == nullptr) return 1;
            data_dir = dir;
        } else {
            data_dir = "sim_data";
        }
    }

    lv_init();
    lv_display_t* display = lv_sdl_window_create(golf::kDisplaySize, golf::kDisplaySize);
    if (display == nullptr) return 1;
    lv_sdl_window_set_title(display, "Golf-Tracker Simulator");
    lv_sdl_mouse_create();

    Simulator simulator;
    simulator.fs.reset(new golf::PosixFileSystem(data_dir));
    if (with_course) {
        // Beim allerersten Start den Beispielplatz ablegen.
        golf::Store store(*simulator.fs);
        golf::Course existing;
        if (!store.loadCourse(existing)) store.saveCourse(golf::demoCourse());
    }
    simulator.session.reset(new golf::Session(*simulator.fs, simulatorRandom));

    if (!replay_path.empty()) {
        std::ifstream in(replay_path, std::ios::binary);
        std::stringstream content;
        content << in.rdbuf();
        simulator.track.reset(new golf::NmeaLog(content.str()));
        if (simulator.track->burstCount() == 0) {
            std::fprintf(stderr, "Keine GPS-Daten in %s\n", replay_path.c_str());
            return 1;
        }
        simulator.feed.reset(new golf::GpsFeed(*simulator.session));
    }

    golf::ui::init(simulator.app());
    simulator.pointer.attach();
    if (!headless) addRoundBezel();
    simulator.apply();

    sim::HttpServer server;
    if (serve_port > 0) {
        if (!server.start(serve_port, web_dir)) {
            std::fprintf(stderr, "Port %d laesst sich nicht oeffnen\n", serve_port);
            return 1;
        }
        char address[40];
        std::snprintf(address, sizeof(address), "127.0.0.1:%d", serve_port);
        simulator.network.set(golf::NetMode::Hotspot, "Golf-Tracker", address);
        std::printf("Web-Oberflaeche: http://127.0.0.1:%d/\n", serve_port);
        std::fflush(stdout);
    }

    if (!simulator.runAll(commands)) return 1;
    if (headless && serve_port > 0) {
        for (;;) { // als Server weiterlaufen, bis der Prozess beendet wird
            if (server.poll(*simulator.session, &simulator.network)) simulator.apply();
            lv_timer_handler();
            SDL_Delay(5);
        }
    }
    if (headless) {
        for (int frame = 0; frame < 20; frame++) lv_timer_handler();
        return 0;
    }

    golf::Button button1, button2;
    golf::ScreenPower power;
    lv_obj_t* veil = addDimVeil();
    golf::ScreenState shown_state = golf::ScreenState::Active;
    bool g_down = false, s_down = false, left_down = false, right_down = false;
    uint32_t next_play_at = SDL_GetTicks();
    for (;;) {
        lv_timer_handler(); // verarbeitet auch die SDL-Ereignisse (Fenster schliessen)
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const uint32_t now = SDL_GetTicks();

        // Wie auf dem Geraet: Ist das Display aus, weckt ein Knopf es nur.
        golf::dispatchButton(simulator.app(), power, 1, button1.update(keys[SDL_SCANCODE_1] != 0, now), now);
        golf::dispatchButton(simulator.app(), power, 2, button2.update(keys[SDL_SCANCODE_2] != 0, now), now);
        int mouse_x = 0, mouse_y = 0;
        if (SDL_GetMouseState(&mouse_x, &mouse_y) & SDL_BUTTON(SDL_BUTTON_LEFT)) power.onInput(now);
        power.tick(now, simulator.app().phoneWanted());
        if (power.state() != shown_state) {
            shown_state = power.state();
            lv_obj_set_style_bg_opa(veil, shown_state == golf::ScreenState::Active ? LV_OPA_TRANSP
                                          : shown_state == golf::ScreenState::Dim ? LV_OPA_60 : LV_OPA_COVER, 0);
        }

        if (keys[SDL_SCANCODE_UP]) simulator.progress_m += 0.4;
        if (keys[SDL_SCANCODE_DOWN]) simulator.progress_m -= 0.4;
        if (simulator.progress_m < -50.0) simulator.progress_m = -50.0;
        if (pressedNow(keys, SDL_SCANCODE_LEFT, left_down)) simulator.app().swipe(-1);
        if (pressedNow(keys, SDL_SCANCODE_RIGHT, right_down)) simulator.app().swipe(1);
        if (pressedNow(keys, SDL_SCANCODE_G, g_down)) simulator.gps_on = !simulator.gps_on;
        if (pressedNow(keys, SDL_SCANCODE_S, s_down)) sim::saveScreenshot("screenshot.bmp");

        if (simulator.track && now >= next_play_at) { // Aufzeichnung im Zeitraffer weiterspielen
            simulator.play(1);
            next_play_at = now + static_cast<uint32_t>(1000.0 / speed);
        }

        if (serve_port > 0) server.poll(*simulator.session, &simulator.network);
        simulator.apply();
        SDL_Delay(5);
    }
}
