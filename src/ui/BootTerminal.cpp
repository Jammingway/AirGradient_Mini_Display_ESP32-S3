#include "BootTerminal.h"
#include "board_pins.h"
#include "../themes/ThemeManager.h"

static constexpr int LINE_H = 26;
static constexpr int PAD = 24;

void BootTerminal::create(const ThemeManager& theme) {
    const ThemePalette& p = theme.palette();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_screen, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < MAX_LINES; i++) {
        lv_obj_t* lbl = lv_label_create(_screen);
        lv_obj_set_style_text_font(lbl, &lv_font_unscii_16, 0);
        lv_obj_set_style_text_color(lbl, p.terminalGreen, 0);
        lv_obj_set_pos(lbl, PAD, PAD + i * LINE_H);
        lv_label_set_text(lbl, "");
        _lineLabels[i] = lbl;
    }
    // Line 0 is the standing "[tap anywhere to configure]" header: centered
    // across the full width and pinned (the log scrolls beneath it).
    lv_obj_set_width(_lineLabels[0], LCD_H_RES);
    lv_obj_set_style_text_align(_lineLabels[0], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(_lineLabels[0], 0, PAD);

    _cursor = lv_label_create(_screen);
    lv_obj_set_style_text_font(_cursor, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_cursor, p.terminalGreen, 0);
    lv_label_set_text(_cursor, "_");

    _hint = lv_label_create(_screen);
    lv_obj_set_style_text_font(_hint, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_hint, p.terminalGreen, 0);
    lv_label_set_text(_hint, "[tap anywhere to configure]");
    lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_add_flag(_hint, LV_OBJ_FLAG_HIDDEN);

    // Dim-green diagnostics line, bottom-left; shown only when debug is on.
    _debugLbl = lv_label_create(_screen);
    lv_obj_set_style_text_font(_debugLbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_debugLbl, lv_color_mix(p.terminalGreen, lv_color_black(), 150), 0);
    lv_label_set_long_mode(_debugLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_debugLbl, LCD_H_RES - 2 * PAD);
    lv_obj_align(_debugLbl, LV_ALIGN_BOTTOM_LEFT, PAD, -6);
    lv_label_set_text(_debugLbl, "");
    lv_obj_add_flag(_debugLbl, LV_OBJ_FLAG_HIDDEN);

    _blinkTimer = lv_timer_create(blinkTimerCb, 530, this);

    lv_obj_add_event_cb(_screen, [](lv_event_t* e) {
        auto* self = static_cast<BootTerminal*>(lv_event_get_user_data(e));
        if (self->_tap) self->_tap();
    }, LV_EVENT_CLICKED, this);

    // First line is the standing invitation to open setup.
    _lines[0] = "[tap anywhere to configure]";
    _count = 1;
    refresh();
}

void BootTerminal::load(bool deletePrev) {
    lv_screen_load_anim(_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, deletePrev);
}

void BootTerminal::pushLine(const String& msg) {
    if (_count == MAX_LINES) {  // scroll the log, keeping the pinned header
        for (int i = 2; i < MAX_LINES; i++) _lines[i - 1] = _lines[i];
        _count--;
    }
    _lines[_count++] = "> " + msg;
    refresh();
}

void BootTerminal::setStatus(const String& msg) {
    if (_count == 0) _count = 1;
    _lines[_count - 1] = "> " + msg;
    refresh();
}

void BootTerminal::showConfigHint(bool show) {
    if (show) lv_obj_remove_flag(_hint, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(_hint, LV_OBJ_FLAG_HIDDEN);
}

void BootTerminal::setDebugLine(const String& text) {
    if (text.length() == 0) {
        lv_obj_add_flag(_debugLbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(_debugLbl, text.c_str());
    lv_obj_remove_flag(_debugLbl, LV_OBJ_FLAG_HIDDEN);
}

void BootTerminal::refresh() {
    for (int i = 0; i < MAX_LINES; i++) {
        lv_label_set_text(_lineLabels[i], i < _count ? _lines[i].c_str() : "");
    }
    int last = _count - 1;
    if (last <= 0) {
        // Only the centered header is showing; blink at the first log row.
        lv_obj_set_pos(_cursor, PAD, PAD + LINE_H);
        return;
    }
    // Cursor sits right after the last character of the active log line.
    lv_obj_update_layout(_lineLabels[last]);
    int w = lv_obj_get_width(_lineLabels[last]);
    lv_obj_set_pos(_cursor, PAD + w + 6, PAD + last * LINE_H);
}

void BootTerminal::blinkTimerCb(lv_timer_t* t) {
    auto* self = static_cast<BootTerminal*>(lv_timer_get_user_data(t));
    if (lv_obj_has_flag(self->_cursor, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(self->_cursor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(self->_cursor, LV_OBJ_FLAG_HIDDEN);
    }
}
