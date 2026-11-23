// Die Ansichten "Schlaege" und "Scorecard".
#pragma once

#include <lvgl.h>

#include "core/display_text.h"

namespace golf {
namespace ui {

class ShotsScreen {
public:
    void create(lv_obj_t* parent);
    void show(const ShotsScreenText& text);

private:
    ShotsScreenText shown_{};
    bool first_ = true;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* empty_ = nullptr;
};

class ScorecardScreen {
public:
    void create(lv_obj_t* parent);
    void show(const ScorecardText& text);

private:
    ScorecardText shown_{};
    bool first_ = true;
    lv_obj_t* subtitle_ = nullptr;
    lv_obj_t* grid_ = nullptr;
    lv_obj_t* cells_[kMaxHoles] = {};
    lv_obj_t* cell_scores_[kMaxHoles] = {};
    lv_obj_t* strokes_ = nullptr;
    lv_obj_t* to_par_ = nullptr;
    lv_obj_t* putts_ = nullptr;
};

// Farbe zu einer Ergebnis-Faerbung.
uint32_t toneColor(ScoreTone tone);

} // namespace ui
} // namespace golf
