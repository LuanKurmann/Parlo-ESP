#include "ui/screen_distance.h"

#include <cstring>

#include "ui/theme.h"

namespace golf {
namespace ui {

namespace {

// Zeile aus Beschriftung und Zahl, z. B. "hinten 282". Liefert das Zahlen-Label.
lv_obj_t* makeEdgeRow(lv_obj_t* parent, const char* caption, int y) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);

    lv_obj_t* label = makeLabel(row, &golf_font_text_26, kColorMuted);
    lv_label_set_text(label, caption);
    lv_obj_set_style_pad_bottom(label, 3, 0); // Grundlinie an die groessere Zahl angleichen
    return makeLabel(row, &golf_font_num_40, kColorText);
}

} // namespace

void DistanceScreen::create(lv_obj_t* parent) {
    // Kopf: GPS-Punkt, Loch, Par und Laenge
    lv_obj_t* head = lv_obj_create(parent);
    lv_obj_remove_style_all(head);
    lv_obj_set_size(head, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(head, 10, 0);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 40);

    gps_dot_ = lv_obj_create(head);
    lv_obj_remove_style_all(gps_dot_);
    lv_obj_set_size(gps_dot_, 9, 9);
    lv_obj_set_style_radius(gps_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(gps_dot_, LV_OPA_COVER, 0);

    title_ = makeLabel(head, &golf_font_cap_22, kColorAccent);
    lv_obj_set_style_text_letter_space(title_, 3, 0);

    subtitle_ = makeLabel(parent, &golf_font_text_20, kColorMuted);
    lv_obj_align(subtitle_, LV_ALIGN_TOP_MID, 0, 70);

    back_ = makeEdgeRow(parent, "hinten", 104);

    // Grosse Zahl mit Einheit rechts unten daneben
    center_ = makeLabel(parent, &golf_font_num_154, kColorText);
    lv_obj_align(center_, LV_ALIGN_CENTER, 0, -6);
    unit_ = makeLabel(parent, &golf_font_text_26, kColorMuted);
    lv_label_set_text(unit_, "m");

    front_ = makeEdgeRow(parent, "vorne", 300);

    hint_ = makeLabel(parent, &golf_font_text_20, kColorWarning);
    lv_obj_align(hint_, LV_ALIGN_TOP_MID, 0, 352);

    foot_ = makeLabel(parent, &golf_font_text_20, kColorMuted);
    lv_obj_align(foot_, LV_ALIGN_TOP_MID, 0, 384);
}

void DistanceScreen::show(const DistanceScreenText& text) {
    if (!first_ && std::memcmp(&text, &shown_, sizeof(text)) == 0) return; // nichts neu zeichnen
    first_ = false;
    shown_ = text;

    lv_label_set_text(title_, text.title);
    lv_label_set_text(subtitle_, text.subtitle);
    lv_label_set_text(back_, text.back);
    lv_label_set_text(center_, text.center);
    lv_label_set_text(front_, text.front);
    lv_label_set_text(hint_, text.hint);
    lv_label_set_text(foot_, text.foot);
    lv_obj_set_style_bg_color(gps_dot_, color(text.gps_ok ? kColorAccent : kColorWarning), 0);

    // Die Einheit wandert mit der Breite der Zahl mit.
    lv_obj_update_layout(center_);
    // Ohne Distanz ("---") gibt es auch keine Einheit.
    lv_obj_align_to(unit_, center_, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 2);
    if (text.center[0] == '-') {
        lv_obj_add_flag(unit_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(unit_, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace ui
} // namespace golf
