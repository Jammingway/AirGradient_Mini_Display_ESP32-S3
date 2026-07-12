#include "InfoWidget.h"

void InfoWidget::create(lv_obj_t* parent, const ThemeManager& theme) {
    const ThemePalette& p = theme.palette();
    _root = makeCard(parent, p);

    _icon = lv_label_create(_root);
    lv_obj_set_style_text_font(_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_icon, p.textMuted, 0);
    lv_label_set_text(_icon, _kind == Kind::Wifi ? LV_SYMBOL_WIFI : LV_SYMBOL_REFRESH);
    lv_obj_align(_icon, LV_ALIGN_TOP_LEFT, 2, 0);

    _nameLbl = lv_label_create(_root);
    lv_obj_set_style_text_font(_nameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_nameLbl, p.textMuted, 0);
    lv_label_set_text(_nameLbl, _kind == Kind::Wifi ? "WiFi" : "Updated");
    lv_obj_align(_nameLbl, LV_ALIGN_TOP_LEFT, 28, 0);

    _valueLbl = lv_label_create(_root);
    lv_obj_set_style_text_font(_valueLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(_valueLbl, p.text, 0);
    lv_label_set_text(_valueLbl, "--");
    lv_label_set_long_mode(_valueLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(_valueLbl, LV_PCT(100));
    lv_obj_align(_valueLbl, LV_ALIGN_BOTTOM_LEFT, 2, -2);
}

void InfoWidget::update(const AirGradientReading&, const AppSettings&, const ThemeManager&) {
    // Content comes via setText() from the dashboard's status tick.
}

void InfoWidget::setText(const String& value, bool alert, const ThemeManager& theme) {
    lv_label_set_text(_valueLbl, value.c_str());
    lv_obj_set_style_text_color(
        _valueLbl, alert ? theme.severityColor(Severity::Bad) : theme.palette().text, 0);
}
