#include "WoprGreeting.h"
#include <cstring>

void WoprGreeting::show(lv_color_t accent, const char* text, DoneCallback cb) {
    _done = std::move(cb);
    _accent = accent;
    _text = text ? text : "";
    _typed = 0;
    _holdElapsed = 0;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _label = lv_label_create(_screen);
    lv_obj_set_style_text_font(_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_label, _accent, 0);
    lv_obj_set_style_text_align(_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(_label, lv_display_get_horizontal_resolution(nullptr) - 60);
    lv_label_set_text(_label, "");
    lv_obj_center(_label);

    lv_screen_load(_screen);
    _timer = lv_timer_create(tickCb, TICK_MS, this);
}

void WoprGreeting::tickCb(lv_timer_t* t) {
    static_cast<WoprGreeting*>(lv_timer_get_user_data(t))->onTick();
}

// Draws the first _typed characters followed by `cursor`.
void WoprGreeting::render(char cursor) {
    char buf[MAX_TEXT];
    size_t n = _typed < sizeof(buf) - 2 ? _typed : sizeof(buf) - 2;
    memcpy(buf, _text, n);
    buf[n] = cursor;
    buf[n + 1] = '\0';
    lv_label_set_text(_label, buf);
}

void WoprGreeting::onTick() {
    size_t len = strlen(_text);
    if (_typed < len) {
        _typed++;
        render('_');
        return;
    }
    _holdElapsed += TICK_MS;
    render(((_holdElapsed / 500) % 2 == 0) ? '_' : ' ');  // blink ~1 Hz
    if (_holdElapsed >= HOLD_MS) finish();
}

void WoprGreeting::finish() {
    if (_timer) {
        lv_timer_delete(_timer);
        _timer = nullptr;
    }
    if (_done) _done();
}
