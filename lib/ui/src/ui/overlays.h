// Was sich ueber die Ansicht legt: Schlaegerwahl, Menue, Loch fertig, Rundenende.
#pragma once

#include <lvgl.h>

#include "core/app.h"
#include "core/display_text.h"

namespace golf {
namespace ui {

class Overlays {
public:
    // `on_input` wird nach jeder Touch-Eingabe aufgerufen.
    void create(lv_obj_t* parent, App& app, void (*on_input)());

    // Zeigt das zum Zustand der App passende Overlay (oder keines).
    void show(const App& app);

private:
    void showOnly(lv_obj_t* active);

    lv_obj_t* club_ = nullptr;
    lv_obj_t* club_subtitle_ = nullptr;
    lv_obj_t* club_hint_ = nullptr;
    lv_obj_t* chips_[kClubCount] = {};
    lv_obj_t* chip_labels_[kClubCount] = {};
    int shown_suggestion_ = -1;

    lv_obj_t* menu_ = nullptr;
    lv_obj_t* items_[kMenuItemCount] = {};
    lv_obj_t* item_labels_[kMenuItemCount] = {};
    int shown_selection_ = -1;

    lv_obj_t* done_ = nullptr;
    lv_obj_t* done_caption_ = nullptr;
    lv_obj_t* done_strokes_ = nullptr;
    lv_obj_t* done_name_ = nullptr;
    lv_obj_t* done_putts_ = nullptr;
    lv_obj_t* done_hint_ = nullptr;

    lv_obj_t* summary_ = nullptr;
    lv_obj_t* summary_strokes_ = nullptr;
    lv_obj_t* summary_line_ = nullptr;
    lv_obj_t* summary_detail_ = nullptr;
    lv_obj_t* summary_hint_ = nullptr;
};

} // namespace ui
} // namespace golf
