#include "CascadeSplash.h"

void CascadeSplash::show(lv_color_t accent, const lv_image_dsc_t* logo,
                         const char* wordmark, DoneCallback cb) {
    _done = std::move(cb);
    _accent = accent;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x0F1115), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    int32_t logoBottom = 12;
    if (logo) {
        lv_obj_t* img = lv_image_create(_screen);
        lv_image_set_src(img, logo);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 12);
        logoBottom = 12 + logo->header.h + 18;
    }
    if (wordmark) {
        lv_obj_t* name = lv_label_create(_screen);
        lv_label_set_text(name, wordmark);
        lv_obj_set_style_text_font(name, &lv_font_unscii_16, 0);
        lv_obj_set_style_text_color(name, _accent, 0);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, logoBottom);
    }

    lv_screen_load(_screen);

    _phase = Phase::Logo;
    _timer = lv_timer_create(tickCb, TICK_MS, this);
}

void CascadeSplash::tickCb(lv_timer_t* t) {
    static_cast<CascadeSplash*>(lv_timer_get_user_data(t))->onTick();
}

void CascadeSplash::onTick() {
    switch (_phase) {
        case Phase::Logo:
            _elapsed += TICK_MS;
            if (_elapsed >= LOGO_MS) _phase = Phase::Cascade;
            break;

        case Phase::Cascade:
            for (int i = 0; i < SPAWN_PER_TICK && _winCount < MAX_WINDOWS; i++) {
                spawnWindow();
            }
            if (_winCount >= MAX_WINDOWS) finish();
            break;
    }
}

void CascadeSplash::spawnWindow() {
    const int32_t horRes = lv_display_get_horizontal_resolution(nullptr);
    const int32_t verRes = lv_display_get_vertical_resolution(nullptr);

    int32_t w = (int32_t)lv_rand(90, 259);
    int32_t h = (int32_t)lv_rand(60, 179);
    int32_t x = (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, horRes - w));
    int32_t y = (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, verRes - h));

    lv_obj_t* win = lv_obj_create(_screen);
    lv_obj_remove_style_all(win);
    lv_obj_set_pos(win, x, y);
    lv_obj_set_size(win, w, h);
    lv_obj_set_style_bg_color(win, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(win, 2, 0);
    lv_obj_set_style_border_color(win, _accent, 0);
    lv_obj_remove_flag(win, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t* bar = lv_obj_create(win);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, 14);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, _accent, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    // A line of "output" inside the window.
    static const char* lines[] = {"> RUN", "SYS ERR", "FORK()", "SEGFAULT",
                                  "> ACCESS", "OVERFLOW", "0xDEAD", "PID 404"};
    lv_obj_t* txt = lv_label_create(win);
    lv_obj_set_style_text_font(txt, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(txt, _accent, 0);
    lv_label_set_text(txt, lines[lv_rand(0, 7)]);
    lv_obj_set_pos(txt, 4, 20);

    _winCount++;
}

void CascadeSplash::finish() {
    if (_timer) {
        lv_timer_delete(_timer);
        _timer = nullptr;
    }
    if (_done) _done();
}
