// PC-Simulator: zeigt die Oberflaeche in einem SDL-Fenster (466 x 466).
//
//   --smoke   rendert kurz und beendet sich mit Code 0 (fuer automatische Tests,
//             laeuft mit SDL_VIDEODRIVER=dummy auch ohne Bildschirm)

#include <cstring>

#include <SDL2/SDL.h>
#include <lvgl.h>

#include "core/app_info.h"
#include "ui/ui.h"

int main(int argc, char** argv) {
    bool smoke = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0) smoke = true;
    }

    lv_init();
    lv_display_t* display = lv_sdl_window_create(golf::kDisplaySize, golf::kDisplaySize);
    if (display == nullptr) return 1;
    lv_sdl_window_set_title(display, "Golf-Tracker Simulator");
    lv_sdl_mouse_create();

    golf::ui::init();

    // Das Fenster-Schliessen behandelt der SDL-Treiber von LVGL selbst.
    for (int frame = 0; !smoke || frame < 50; frame++) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
