#include "ui/views.h"

#include <cstring>

#include "ui/theme.h"

namespace golf {
namespace ui {

uint32_t toneColor(ScoreTone tone) {
    switch (tone) {
        case ScoreTone::Under: return kColorAccent;
        case ScoreTone::Over: return kColorWarning;
        case ScoreTone::None: return kColorMuted;
        case ScoreTone::Par: break;
    }
    return kColorText;
}

// ---------- Schlaege ----------

namespace {
constexpr int kListWidth = 300;
constexpr int kListHeight = 268;
} // namespace

void ShotsScreen::create(lv_obj_t* parent) {
    title_ = makeCaption(parent, 40);

    lv_obj_t* subtitle = makeLabel(parent, &golf_font_text_20, kColorMuted);
    lv_label_set_text(subtitle, "Schläge");
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 70);

    // Liste; bei vielen Schlaegen laesst sie sich senkrecht verschieben.
    list_ = lv_obj_create(parent);
    lv_obj_remove_style_all(list_);
    lv_obj_set_size(list_, kListWidth, kListHeight);
    lv_obj_align(list_, LV_ALIGN_TOP_MID, 0, 116);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_GESTURE_BUBBLE);

    empty_ = makeLabel(parent, &golf_font_text_20, kColorMuted);
    lv_label_set_text(empty_, "Noch kein Schlag.\nAm Ball Knopf 1 drücken.");
    lv_obj_set_style_text_align(empty_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(empty_, LV_ALIGN_CENTER, 0, 10);
}

void ShotsScreen::show(const ShotsScreenText& text) {
    if (!first_ && std::memcmp(&text, &shown_, sizeof(text)) == 0) return;
    const bool count_changed = first_ || text.count != shown_.count;
    first_ = false;
    shown_ = text;

    lv_label_set_text(title_, text.title);
    if (text.count == 0) {
        lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(list_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(list_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clean(list_);
    for (int i = 0; i < text.count; i++) {
        const ShotRowText& row_text = text.rows[i];
        lv_obj_t* row = makeBox(list_);
        lv_obj_set_size(row, kListWidth, 38);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, color(kColorLine), 0);

        lv_obj_t* number = makeLabel(row, &golf_font_text_20, kColorMuted);
        lv_label_set_text(number, row_text.number);
        lv_obj_align(number, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* club = makeLabel(row, &golf_font_text_26, kColorText);
        lv_label_set_text(club, row_text.club);
        lv_obj_align(club, LV_ALIGN_LEFT_MID, 34, 0);

        lv_obj_t* distance = makeLabel(row, &golf_font_num_30, kColorText);
        lv_label_set_text(distance, row_text.distance);
        lv_obj_align(distance, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    // Wenige Zeilen stehen mittig; viele beginnen oben und der neueste Schlag ist sichtbar.
    lv_obj_update_layout(list_);
    const bool overflows = text.count * 38 > kListHeight;
    lv_obj_set_flex_align(list_, overflows ? LV_FLEX_ALIGN_START : LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    if (count_changed && text.count > 0) {
        lv_obj_update_layout(list_);
        lv_obj_scroll_to_view(lv_obj_get_child(list_, text.count - 1), LV_ANIM_OFF);
    }
}

// ---------- Scorecard ----------

namespace {
constexpr int kCellWidth = 51;
constexpr int kCellHeight = 60;
constexpr int kCellGap = 7;
constexpr int kColumns = 6;
} // namespace

void ScorecardScreen::create(lv_obj_t* parent) {
    lv_obj_t* title = makeCaption(parent, 40);
    lv_label_set_text(title, "SCORECARD");
    subtitle_ = makeLabel(parent, &golf_font_text_20, kColorMuted);
    lv_obj_align(subtitle_, LV_ALIGN_TOP_MID, 0, 70);

    grid_ = makeBox(parent);
    lv_obj_set_width(grid_, kColumns * kCellWidth + (kColumns - 1) * kCellGap);
    lv_obj_set_height(grid_, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(grid_, kCellGap, 0);
    lv_obj_set_style_pad_row(grid_, kCellGap, 0);
    lv_obj_align(grid_, LV_ALIGN_CENTER, 0, -8);

    for (int h = 0; h < kMaxHoles; h++) {
        lv_obj_t* cell = makeBox(grid_);
        lv_obj_set_size(cell, kCellWidth, kCellHeight);
        lv_obj_set_style_radius(cell, 10, 0);
        lv_obj_set_style_bg_color(cell, color(kColorChip), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_set_style_border_color(cell, color(kColorAccent), 0);
        lv_obj_set_style_border_opa(cell, LV_OPA_TRANSP, 0);

        lv_obj_t* hole = makeLabel(cell, &golf_font_text_16, kColorMuted);
        lv_label_set_text_fmt(hole, "%d", h + 1);
        lv_obj_align(hole, LV_ALIGN_TOP_MID, 0, 1);

        cell_scores_[h] = makeLabel(cell, &golf_font_num_30, kColorText);
        lv_obj_align(cell_scores_[h], LV_ALIGN_BOTTOM_MID, 0, -1);
        cells_[h] = cell;
    }

    // Fusszeile aus drei Teilen, damit der Stand gegen Par eingefaerbt werden kann.
    lv_obj_t* foot = makeBox(parent);
    lv_obj_set_size(foot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(foot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(foot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(foot, 8, 0);
    lv_obj_align(foot, LV_ALIGN_TOP_MID, 0, 372);
    strokes_ = makeLabel(foot, &golf_font_text_20, kColorText);
    lv_obj_t* dot1 = makeLabel(foot, &golf_font_text_20, kColorMuted);
    lv_label_set_text(dot1, "·");
    to_par_ = makeLabel(foot, &golf_font_text_20, kColorText);
    lv_obj_t* dot2 = makeLabel(foot, &golf_font_text_20, kColorMuted);
    lv_label_set_text(dot2, "·");
    putts_ = makeLabel(foot, &golf_font_text_20, kColorText);
}

void ScorecardScreen::show(const ScorecardText& text) {
    if (!first_ && std::memcmp(&text, &shown_, sizeof(text)) == 0) return;
    first_ = false;
    shown_ = text;

    lv_label_set_text(subtitle_, text.subtitle);
    for (int h = 0; h < kMaxHoles; h++) {
        if (h >= text.hole_count) {
            lv_obj_add_flag(cells_[h], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(cells_[h], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(cell_scores_[h], text.cells[h].score);
        lv_obj_set_style_text_color(cell_scores_[h], color(toneColor(text.cells[h].tone)), 0);
        lv_obj_set_style_border_opa(cells_[h], text.cells[h].current ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    lv_label_set_text(strokes_, text.strokes);
    lv_label_set_text(to_par_, text.to_par);
    lv_obj_set_style_text_color(to_par_, color(toneColor(text.to_par_tone)), 0);
    lv_label_set_text(putts_, text.putts);
}

} // namespace ui
} // namespace golf
