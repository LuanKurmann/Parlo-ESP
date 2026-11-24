#include "ui/overlays.h"

#include <cstdint>
#include <initializer_list>

#include "ui/theme.h"
#include "ui/views.h"

namespace golf {
namespace ui {

namespace {

App* g_app = nullptr;
void (*g_on_input)() = nullptr;

void notify() {
    if (g_on_input != nullptr) g_on_input();
}

void onClubClicked(lv_event_t* e) {
    const intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    g_app->selectClub(static_cast<Club>(index));
    notify();
}

void onMenuClicked(lv_event_t* e) {
    const intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    g_app->selectMenuItem(static_cast<MenuItem>(index));
    notify();
}

void onDoneClicked(lv_event_t*) {
    g_app->tap();
    notify();
}

// Deckende schwarze Flaeche ueber dem ganzen Display.
lv_obj_t* makeOverlay(lv_obj_t* parent) {
    lv_obj_t* overlay = makeBox(parent);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, color(kColorBackground), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE); // faengt Beruehrungen ab
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_GESTURE_BUBBLE); // kein Wischen unter Overlays
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    return overlay;
}

lv_obj_t* makeCenteredLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t text_color, int y_from_center) {
    lv_obj_t* label = makeLabel(parent, font, text_color);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y_from_center);
    return label;
}

// Kachel zum Antippen mit einem Text darin.
lv_obj_t* makeTile(lv_obj_t* parent, int width, int height, const lv_font_t* font, lv_obj_t** label_out) {
    lv_obj_t* tile = makeBox(parent);
    lv_obj_set_size(tile, width, height);
    lv_obj_set_style_radius(tile, 14, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    *label_out = makeLabel(tile, font, kColorText);
    lv_obj_center(*label_out);
    return tile;
}

} // namespace

void Overlays::create(lv_obj_t* parent, App& app, void (*on_input)()) {
    g_app = &app;
    g_on_input = on_input;

    // ---------- Schlaegerwahl ----------
    club_ = makeOverlay(parent);
    lv_obj_t* club_caption = makeCaption(club_, 78);
    lv_label_set_text(club_caption, "SCHLÄGER");
    club_subtitle_ = makeLabel(club_, &golf_font_text_20, kColorMuted);
    lv_obj_align(club_subtitle_, LV_ALIGN_TOP_MID, 0, 110);

    lv_obj_t* chip_grid = makeBox(club_);
    lv_obj_set_size(chip_grid, 4 * 77 + 3 * 8, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chip_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(chip_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip_grid, 8, 0);
    lv_obj_set_style_pad_row(chip_grid, 8, 0);
    lv_obj_align(chip_grid, LV_ALIGN_TOP_MID, 0, 150);
    for (int i = 0; i < kClubCount; i++) {
        chips_[i] = makeTile(chip_grid, 77, 56, &golf_font_num_30, &chip_labels_[i]);
        lv_label_set_text(chip_labels_[i], clubShortName(static_cast<Club>(i)));
        lv_obj_add_event_cb(chips_[i], onClubClicked, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }
    club_hint_ = makeLabel(club_, &golf_font_text_20, kColorMuted);
    lv_obj_align(club_hint_, LV_ALIGN_TOP_MID, 0, 352);

    // ---------- Menue ----------
    menu_ = makeOverlay(parent);
    lv_obj_t* menu_caption = makeCaption(menu_, 52);
    lv_label_set_text(menu_caption, "MENÜ");
    lv_obj_t* item_column = makeBox(menu_);
    lv_obj_set_size(item_column, 300, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(item_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(item_column, 8, 0);
    lv_obj_align(item_column, LV_ALIGN_TOP_MID, 0, 90);
    for (int i = 0; i < kMenuItemCount; i++) {
        items_[i] = makeTile(item_column, 300, 50, &golf_font_text_26, &item_labels_[i]);
        lv_label_set_text(item_labels_[i], menuItemText(static_cast<MenuItem>(i)));
        lv_obj_set_style_bg_color(items_[i], color(kColorChip), 0);
        lv_obj_add_event_cb(items_[i], onMenuClicked, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    // ---------- Loch fertig ----------
    done_ = makeOverlay(parent);
    lv_obj_add_event_cb(done_, onDoneClicked, LV_EVENT_CLICKED, nullptr);
    done_caption_ = makeCaption(done_, 86);
    done_strokes_ = makeCenteredLabel(done_, &golf_font_num_100, kColorText, -40);
    done_name_ = makeCenteredLabel(done_, &golf_font_num_40, kColorText, 42);
    done_putts_ = makeCenteredLabel(done_, &golf_font_text_20, kColorMuted, 84);
    done_hint_ = makeCenteredLabel(done_, &golf_font_text_16, kColorMuted, 134);

    // ---------- Rundenende ----------
    summary_ = makeOverlay(parent);
    lv_obj_t* summary_caption = makeCaption(summary_, 78);
    lv_label_set_text(summary_caption, "RUNDE");
    summary_strokes_ = makeCenteredLabel(summary_, &golf_font_num_100, kColorText, -48);
    summary_line_ = makeCenteredLabel(summary_, &golf_font_num_40, kColorText, 32);
    summary_detail_ = makeCenteredLabel(summary_, &golf_font_text_20, kColorMuted, 76);
    summary_hint_ = makeCenteredLabel(summary_, &golf_font_text_16, kColorMuted, 132);

    // ---------- Mit Handy verbinden ----------
    phone_ = makeOverlay(parent);
    lv_obj_add_event_cb(phone_, onDoneClicked, LV_EVENT_CLICKED, nullptr); // Tippen schliesst
    lv_obj_t* phone_caption = makeCaption(phone_, 78);
    lv_label_set_text(phone_caption, "HANDY");
    phone_line1_ = makeCenteredLabel(phone_, &golf_font_text_20, kColorMuted, -96);
    phone_value1_ = makeCenteredLabel(phone_, &golf_font_num_40, kColorText, -62);
    phone_detail1_ = makeCenteredLabel(phone_, &golf_font_text_20, kColorText, -26);
    phone_line2_ = makeCenteredLabel(phone_, &golf_font_text_20, kColorMuted, 22);
    phone_value2_ = makeCenteredLabel(phone_, &golf_font_num_40, kColorAccent, 56);
    phone_hint_ = makeCenteredLabel(phone_, &golf_font_text_16, kColorMuted, 132);
    // Lange WLAN-Namen und Adressen werden mit "..." gekuerzt statt ueber den Rand zu laufen.
    for (lv_obj_t* value : {phone_value1_, phone_value2_}) {
        lv_obj_set_width(value, 380);
        lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
    }
}

void Overlays::showOnly(lv_obj_t* active) {
    lv_obj_t* all[] = {club_, menu_, done_, summary_, phone_};
    for (lv_obj_t* overlay : all) {
        if (overlay == active) {
            lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Overlays::show(const App& app) {
    switch (app.overlay()) {
        case Overlay::None:
            showOnly(nullptr);
            break;

        case Overlay::ClubPicker: {
            const ClubPickerText text = clubPickerText(app);
            lv_label_set_text(club_subtitle_, text.subtitle);
            lv_label_set_text(club_hint_, text.hint);
            if (text.suggested != shown_suggestion_) {
                shown_suggestion_ = text.suggested;
                for (int i = 0; i < kClubCount; i++) {
                    const bool on = i == text.suggested;
                    lv_obj_set_style_bg_color(chips_[i], color(on ? kColorAccent : kColorChip), 0);
                    lv_obj_set_style_border_color(chips_[i], color(on ? kColorAccent : kColorLine), 0);
                    lv_obj_set_style_text_color(chip_labels_[i], color(on ? kColorOnAccent : kColorText), 0);
                }
            }
            showOnly(club_);
            break;
        }

        case Overlay::Menu: {
            const int selected = static_cast<int>(app.menuSelection());
            if (selected != shown_selection_) {
                shown_selection_ = selected;
                for (int i = 0; i < kMenuItemCount; i++) {
                    const bool on = i == selected;
                    lv_obj_set_style_border_color(items_[i], color(on ? kColorAccent : kColorLine), 0);
                    lv_obj_set_style_border_width(items_[i], on ? 2 : 1, 0);
                    lv_obj_set_style_text_color(item_labels_[i], color(on ? kColorAccent : kColorText), 0);
                }
            }
            showOnly(menu_);
            break;
        }

        case Overlay::HoleDone: {
            const HoleDoneText text = holeDoneText(app);
            lv_label_set_text(done_caption_, text.caption);
            lv_label_set_text(done_strokes_, text.strokes);
            lv_obj_set_style_text_color(done_strokes_, color(toneColor(text.tone)), 0);
            lv_label_set_text(done_name_, text.name);
            lv_label_set_text(done_putts_, text.putts);
            lv_label_set_text(done_hint_, text.hint);
            showOnly(done_);
            break;
        }

        case Overlay::Phone: {
            const PhoneText text = phoneText(app);
            lv_label_set_text(phone_line1_, text.line1);
            lv_label_set_text(phone_value1_, text.value1);
            lv_label_set_text(phone_detail1_, text.detail1);
            lv_label_set_text(phone_line2_, text.line2);
            lv_label_set_text(phone_value2_, text.value2);
            lv_label_set_text(phone_hint_, text.hint);
            showOnly(phone_);
            break;
        }

        case Overlay::RoundSummary: {
            const RoundSummaryText text = roundSummaryText(app);
            lv_label_set_text(summary_strokes_, text.strokes);
            lv_label_set_text(summary_line_, text.line);
            lv_obj_set_style_text_color(summary_line_, color(toneColor(text.tone)), 0);
            lv_label_set_text(summary_detail_, text.detail);
            lv_label_set_text(summary_hint_, text.hint);
            showOnly(summary_);
            break;
        }
    }
}

} // namespace ui
} // namespace golf
