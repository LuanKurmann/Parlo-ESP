#include "ui/ui.h"

#include <lvgl.h>

#include "core/display_text.h"
#include "ui/screen_distance.h"
#include "ui/theme.h"

namespace golf {
namespace ui {

namespace {
App* g_app = nullptr;
DistanceScreen g_distance;
} // namespace

void init(App& app) {
    g_app = &app;

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, color(kColorBackground), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    g_distance = DistanceScreen();
    g_distance.create(screen);
    refresh();
}

void refresh() {
    if (g_app == nullptr) return;
    g_distance.show(distanceScreenText(*g_app));
}

} // namespace ui
} // namespace golf
