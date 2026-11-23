#include "ui/ui.h"

#include <lvgl.h>

#include "core/app_info.h"

namespace golf {
namespace ui {

void init() {
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Golf-Tracker");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x5FE08A), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t* ver = lv_label_create(screen);
    lv_label_set_text_fmt(ver, "Version %s", version());
    lv_obj_set_style_text_font(ver, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x98A39C), 0);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, 20);
}

} // namespace ui
} // namespace golf
