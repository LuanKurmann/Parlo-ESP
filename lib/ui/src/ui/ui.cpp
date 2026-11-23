#include "ui/ui.h"

#include <lvgl.h>

#include "core/display_text.h"
#include "ui/overlays.h"
#include "ui/screen_distance.h"
#include "ui/theme.h"
#include "ui/views.h"

namespace golf {
namespace ui {

namespace {

constexpr uint32_t kToastMs = 1800;

App* g_app = nullptr;
DistanceScreen g_distance;
ShotsScreen g_shots;
ScorecardScreen g_scorecard;
Overlays g_overlays;

lv_obj_t* g_views[kViewCount] = {};
lv_obj_t* g_dots[kViewCount] = {};
lv_obj_t* g_dot_row = nullptr;
int g_shown_view = -1;

lv_obj_t* g_toast = nullptr;
lv_obj_t* g_toast_label = nullptr;
lv_timer_t* g_toast_timer = nullptr;

void onGesture(lv_event_t*) {
    lv_indev_t* indev = lv_indev_active();
    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    // Ein langes Wischen meldet LVGL sonst mehrfach; pro Beruehrung zaehlt es einmal.
    lv_indev_wait_release(indev);
    if (direction == LV_DIR_LEFT) {
        g_app->swipe(1);
    } else if (direction == LV_DIR_RIGHT) {
        g_app->swipe(-1);
    } else {
        return;
    }
    refresh();
}

void hideToast(lv_timer_t*) {
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(g_toast_timer);
}

void showToast(const char* text) {
    lv_label_set_text(g_toast_label, text);
    lv_obj_remove_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(g_toast_timer);
    lv_timer_resume(g_toast_timer);
}

lv_obj_t* makeView(lv_obj_t* screen) {
    lv_obj_t* view = makeBox(screen);
    lv_obj_set_size(view, LV_PCT(100), LV_PCT(100));
    return view;
}

} // namespace

void init(App& app) {
    g_app = &app;
    g_shown_view = -1;

    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, color(kColorBackground), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, onGesture, LV_EVENT_GESTURE, nullptr);

    // Ansichten
    g_distance = DistanceScreen();
    g_shots = ShotsScreen();
    g_scorecard = ScorecardScreen();
    g_views[0] = makeView(screen);
    g_distance.create(g_views[0]);
    g_views[1] = makeView(screen);
    g_shots.create(g_views[1]);
    g_views[2] = makeView(screen);
    g_scorecard.create(g_views[2]);

    // Punkte unten: welche der drei Ansichten offen ist
    g_dot_row = makeBox(screen);
    lv_obj_set_size(g_dot_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(g_dot_row, 9, 0);
    lv_obj_align(g_dot_row, LV_ALIGN_TOP_MID, 0, 420);
    for (int i = 0; i < kViewCount; i++) {
        g_dots[i] = makeBox(g_dot_row);
        lv_obj_set_size(g_dots[i], 9, 9);
        lv_obj_set_style_radius(g_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(g_dots[i], LV_OPA_COVER, 0);
    }

    g_overlays = Overlays();
    g_overlays.create(screen, app, refresh);

    // Einblendung fuer kurze Rueckmeldungen
    g_toast = makeBox(screen);
    lv_obj_set_size(g_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(g_toast, color(kColorChip), 0);
    lv_obj_set_style_bg_opa(g_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_toast, 1, 0);
    lv_obj_set_style_border_color(g_toast, color(kColorAccent), 0);
    lv_obj_set_style_radius(g_toast, 18, 0);
    lv_obj_set_style_pad_hor(g_toast, 22, 0);
    lv_obj_set_style_pad_ver(g_toast, 14, 0);
    lv_obj_center(g_toast);
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    g_toast_label = makeLabel(g_toast, &golf_font_text_20, kColorText);
    g_toast_timer = lv_timer_create(hideToast, kToastMs, nullptr);
    lv_timer_pause(g_toast_timer);

    refresh();
}

void refresh() {
    if (g_app == nullptr) return;
    const App& app = *g_app;

    const int view = static_cast<int>(app.view());
    if (view != g_shown_view) {
        g_shown_view = view;
        for (int i = 0; i < kViewCount; i++) {
            if (i == view) {
                lv_obj_remove_flag(g_views[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(g_views[i], LV_OBJ_FLAG_HIDDEN);
            }
            lv_obj_set_style_bg_color(g_dots[i], color(i == view ? kColorAccent : kColorLine), 0);
        }
    }

    // Nur die sichtbare Ansicht wird aufbereitet.
    switch (app.view()) {
        case View::Distance: g_distance.show(distanceScreenText(app)); break;
        case View::Shots: g_shots.show(shotsScreenText(app)); break;
        case View::Scorecard: g_scorecard.show(scorecardText(app)); break;
    }
    g_overlays.show(app);

    const Message message = g_app->takeMessage();
    if (message != Message::None) showToast(messageText(message));
}

} // namespace ui
} // namespace golf
