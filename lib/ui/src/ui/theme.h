// Farben, Schriften und Masse der Oberflaeche.
#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(golf_font_text_16)
LV_FONT_DECLARE(golf_font_text_20)
LV_FONT_DECLARE(golf_font_text_26)
LV_FONT_DECLARE(golf_font_cap_22)
LV_FONT_DECLARE(golf_font_num_30)
LV_FONT_DECLARE(golf_font_num_40)
LV_FONT_DECLARE(golf_font_num_100)
LV_FONT_DECLARE(golf_font_num_154)

namespace golf {
namespace ui {

// Reines Schwarz als Hintergrund: Auf dem AMOLED leuchten schwarze Pixel nicht.
constexpr uint32_t kColorBackground = 0x000000;
constexpr uint32_t kColorText = 0xFFFFFF;
constexpr uint32_t kColorMuted = 0x98A39C;
constexpr uint32_t kColorAccent = 0x5FE08A;
constexpr uint32_t kColorWarning = 0xFFB84D;
constexpr uint32_t kColorLine = 0x1E2822;
constexpr uint32_t kColorChip = 0x121814;       // Flaeche von Kacheln und Knoepfen
constexpr uint32_t kColorOnAccent = 0x04210F;   // Text auf gruener Flaeche

inline lv_color_t color(uint32_t hex) { return lv_color_hex(hex); }

// Legt ein Label mit Schrift und Farbe an.
inline lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t text_color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(text_color), 0);
    lv_label_set_text(label, "");
    return label;
}

// Durchsichtiger Behaelter ohne eigenes Aussehen.
inline lv_obj_t* makeBox(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    return box;
}

// Ueberschrift in Grossbuchstaben mit Sperrung, oben in der Mitte.
inline lv_obj_t* makeCaption(lv_obj_t* parent, int y) {
    lv_obj_t* label = makeLabel(parent, &golf_font_cap_22, kColorAccent);
    lv_obj_set_style_text_letter_space(label, 3, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

} // namespace ui
} // namespace golf
