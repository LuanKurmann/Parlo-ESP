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
//
//   --data      Ordner fuer Platz und Runden (Standard: sim_data)
//   --headless  ohne Fenster; beendet sich nach den Befehlen. Ohne --data
//               beginnt jeder Lauf mit leerem Speicher.
//   --do        Befehle, mit Komma getrennt, werden beim Start ausgefuehrt:
//                 b1 b2        Knopf kurz        B1 B2   Knopf lang
//                 pos:218      218 m vom Abschlag entfernt stehen
//                 gps:off / gps:on
//                 click:233:300    Display an der Stelle x:y antippen
//                 swipe:left / swipe:right   ueber das Display wischen
//                 wait:2000        2 Sekunden warten (z. B. bis eine Einblendung weg ist)
//                 shot:datei.bmp   Screenshot speichern
//
// Beispiel: program --headless --do "b1,b1,pos:218,shot:distanz.bmp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <unistd.h>

#include "core/app_info.h"
#include "core/button.h"
#include "core/posix_fs.h"
#include "core/session.h"
#include "sample_course.h"
#include "screenshot.h"
#include "ui/ui.h"

namespace {

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
    double progress_m = 0.0; // Entfernung vom Abschlag auf der Bahn
    bool gps_on = true;
    int hole = -1;

    golf::App& app() { return session->app(); }

    // Uebertraegt die simulierte Position in die App und sichert den Stand.
    void apply() {
        if (app().currentHole() != hole) { // neues Loch: zurueck an den Abschlag
            hole = app().currentHole();
            progress_m = 0.0;
        }
        if (gps_on) {
            app().setPosition(sim::positionOnHole(app().course().holes[hole], progress_m));
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
        } else if (command.compare(0, 5, "wait:") == 0) {
            apply();
            pump(static_cast<uint32_t>(std::atoi(command.c_str() + 5)));
        } else if (command.compare(0, 5, "shot:") == 0) {
            apply();
            lv_timer_handler();
            return sim::saveScreenshot(command.substr(5));
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
    std::string data_dir;
    std::string commands;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--headless" || arg == "--smoke") {
            headless = true;
        } else if (arg == "--data" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--do" && i + 1 < argc) {
            commands = argv[++i];
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
    {
        // Beim allerersten Start den Beispielplatz ablegen.
        golf::Store store(*simulator.fs);
        golf::Course existing;
        if (!store.loadCourse(existing)) store.saveCourse(sim::sampleCourse());
    }
    simulator.session.reset(new golf::Session(*simulator.fs));

    golf::ui::init(simulator.app());
    simulator.pointer.attach();
    if (!headless) addRoundBezel();
    simulator.apply();

    if (!simulator.runAll(commands)) return 1;
    if (headless) {
        for (int frame = 0; frame < 20; frame++) lv_timer_handler();
        return 0;
    }

    golf::Button button1, button2;
    bool g_down = false, s_down = false, left_down = false, right_down = false;
    for (;;) {
        lv_timer_handler(); // verarbeitet auch die SDL-Ereignisse (Fenster schliessen)
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        const uint32_t now = SDL_GetTicks();

        golf::dispatchButton(simulator.app(), 1, button1.update(keys[SDL_SCANCODE_1] != 0, now));
        golf::dispatchButton(simulator.app(), 2, button2.update(keys[SDL_SCANCODE_2] != 0, now));

        if (keys[SDL_SCANCODE_UP]) simulator.progress_m += 0.4;
        if (keys[SDL_SCANCODE_DOWN]) simulator.progress_m -= 0.4;
        if (simulator.progress_m < -50.0) simulator.progress_m = -50.0;
        if (pressedNow(keys, SDL_SCANCODE_LEFT, left_down)) simulator.app().swipe(-1);
        if (pressedNow(keys, SDL_SCANCODE_RIGHT, right_down)) simulator.app().swipe(1);
        if (pressedNow(keys, SDL_SCANCODE_G, g_down)) simulator.gps_on = !simulator.gps_on;
        if (pressedNow(keys, SDL_SCANCODE_S, s_down)) sim::saveScreenshot("screenshot.bmp");

        simulator.apply();
        SDL_Delay(5);
    }
}
