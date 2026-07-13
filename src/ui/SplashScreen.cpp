#include "SplashScreen.h"
#include <cstring>
#include <esp_system.h>   // esp_random()
#include "board_pins.h"
#include "../themes/ThemeManager.h"
#include "../assets/panda_img.h"

void SplashScreen::show(const ThemeManager& theme, DoneCallback cb) {
    _done = std::move(cb);
    _green = theme.palette().terminalGreen;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x0F1115), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _img = lv_image_create(_screen);
    lv_image_set_src(_img, &panda_img);
    lv_obj_align(_img, LV_ALIGN_TOP_MID, 0, 12);

    _name = lv_label_create(_screen);
    lv_label_set_text(_name, "<Jammingway presents...>");
    lv_obj_set_style_text_font(_name, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_name, _green, 0);
    lv_obj_align(_name, LV_ALIGN_TOP_MID, 0, 12 + 400 + 18);

    lv_screen_load(_screen);

    _phase = Phase::Logo;
    _timer = lv_timer_create(tickCb, TICK_MS, this);
}

void SplashScreen::tickCb(lv_timer_t* t) {
    static_cast<SplashScreen*>(lv_timer_get_user_data(t))->onTick();
}

void SplashScreen::onTick() {
    switch (_phase) {
        case Phase::Logo:
            _elapsed += TICK_MS;
            if (_elapsed >= LOGO_MS) _phase = Phase::Cascade;
            break;

        case Phase::Cascade:
            for (int i = 0; i < SPAWN_PER_TICK && _winCount < MAX_WINDOWS; i++) {
                spawnWindow();
            }
            if (_winCount >= MAX_WINDOWS) {
                startWopr();
                _phase = Phase::Wopr;
            }
            break;

        case Phase::Wopr: {
            size_t len = strlen(WOPR_TEXT);
            if (_typed < len) {
                _typed++;
                char buf[80];
                size_t n = _typed < sizeof(buf) - 2 ? _typed : sizeof(buf) - 2;
                memcpy(buf, WOPR_TEXT, n);
                buf[n] = '_';        // typing cursor
                buf[n + 1] = '\0';
                lv_label_set_text(_wopr, buf);
            } else {
                _holdElapsed += TICK_MS;
                // Blink the trailing cursor ~1 Hz while holding.
                char buf[80];
                size_t n = len < sizeof(buf) - 2 ? len : sizeof(buf) - 2;
                memcpy(buf, WOPR_TEXT, n);
                buf[n] = ((_holdElapsed / 500) % 2 == 0) ? '_' : ' ';
                buf[n + 1] = '\0';
                lv_label_set_text(_wopr, buf);
                if (_holdElapsed >= WOPR_HOLD_MS) finish();
            }
            break;
        }
    }
}

void SplashScreen::spawnWindow() {
    int w = 90 + (int)(esp_random() % 170);   // 90..259
    int h = 60 + (int)(esp_random() % 120);   // 60..179
    int x = (int)(esp_random() % (uint32_t)max(1, LCD_H_RES - w));
    int y = (int)(esp_random() % (uint32_t)max(1, LCD_V_RES - h));

    lv_obj_t* win = lv_obj_create(_screen);
    lv_obj_remove_style_all(win);
    lv_obj_set_pos(win, x, y);
    lv_obj_set_size(win, w, h);
    lv_obj_set_style_bg_color(win, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(win, 2, 0);
    lv_obj_set_style_border_color(win, _green, 0);
    lv_obj_remove_flag(win, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar
    lv_obj_t* bar = lv_obj_create(win);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, 14);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, _green, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    // A line of "output" inside the window.
    static const char* lines[] = {"> RUN", "SYS ERR", "FORK()", "SEGFAULT",
                                   "> ACCESS", "OVERFLOW", "0xDEAD", "PID 404"};
    lv_obj_t* txt = lv_label_create(win);
    lv_obj_set_style_text_font(txt, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(txt, _green, 0);
    lv_label_set_text(txt, lines[esp_random() % 8]);
    lv_obj_set_pos(txt, 4, 20);

    _winCount++;
}

void SplashScreen::startWopr() {
    // Clear the cascade (and logo) in one shot, reclaiming all those objects.
    lv_obj_clean(_screen);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    _img = nullptr;
    _name = nullptr;

    _wopr = lv_label_create(_screen);
    lv_obj_set_style_text_font(_wopr, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_wopr, _green, 0);
    lv_obj_set_style_text_align(_wopr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(_wopr, LCD_H_RES - 60);
    lv_label_set_text(_wopr, "");
    lv_obj_center(_wopr);
    _typed = 0;
    _holdElapsed = 0;
}

void SplashScreen::finish() {
    if (_timer) {
        lv_timer_delete(_timer);
        _timer = nullptr;
    }
    if (_done) _done();  // loads the next screen (with deletePrev to free this)
}
