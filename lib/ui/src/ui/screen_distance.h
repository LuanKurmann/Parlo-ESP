// Hauptansicht: grosse Distanz zur Gruenmitte, darueber hinten, darunter vorne.
#pragma once

#include <lvgl.h>

#include "core/display_text.h"

namespace golf {
namespace ui {

class DistanceScreen {
public:
    void create(lv_obj_t* parent);
    void show(const DistanceScreenText& text);

private:
    DistanceScreenText shown_{};
    bool first_ = true;
    lv_obj_t* gps_dot_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* subtitle_ = nullptr;
    lv_obj_t* back_ = nullptr;
    lv_obj_t* center_ = nullptr;
    lv_obj_t* unit_ = nullptr;
    lv_obj_t* front_ = nullptr;
    lv_obj_t* hint_ = nullptr;
    lv_obj_t* foot_ = nullptr;
};

} // namespace ui
} // namespace golf
