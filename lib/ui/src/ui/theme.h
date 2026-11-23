// Farben, Schriften und Masse der Oberflaeche.
#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(golf_font_text_20)
LV_FONT_DECLARE(golf_font_text_26)
LV_FONT_DECLARE(golf_font_cap_22)
LV_FONT_DECLARE(golf_font_num_40)
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

inline lv_color_t color(uint32_t hex) { return lv_color_hex(hex); }

// Legt ein Label mit Schrift und Farbe an.
inline lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t text_color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(text_color), 0);
    lv_label_set_text(label, "");
    return label;
}

} // namespace ui
} // namespace golf
