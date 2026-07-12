#include "SplashScreen.h"
#include "../themes/ThemeManager.h"
#include "../assets/panda_img.h"

void SplashScreen::show(const ThemeManager& theme, DoneCallback cb) {
    _done = std::move(cb);
    const ThemePalette& p = theme.palette();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x0F1115), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* img = lv_image_create(_screen);
    lv_image_set_src(img, &panda_img);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t* name = lv_label_create(_screen);
    lv_label_set_text(name, "<syntax_error>");
    lv_obj_set_style_text_font(name, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(name, p.terminalGreen, 0);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 36 + 320 + 28);

    lv_screen_load(_screen);

    // Hold, then blank the artwork (fade to black) and hand off.
    _img = img;
    _name = name;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, FADE_MS);
    lv_anim_set_delay(&a, HOLD_MS);
    lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
        auto* self = static_cast<SplashScreen*>(var);
        lv_obj_set_style_opa(self->_img, (lv_opa_t)v, 0);
        lv_obj_set_style_opa(self->_name, (lv_opa_t)v, 0);
    });
    lv_anim_set_user_data(&a, this);
    lv_anim_set_completed_cb(&a, [](lv_anim_t* anim) {
        auto* self = static_cast<SplashScreen*>(lv_anim_get_user_data(anim));
        if (self->_done) self->_done();
    });
    lv_anim_start(&a);
}
