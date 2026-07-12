#include "MetricWidget.h"

// ---- severity rules ------------------------------------------------------
static Severity sevPm25(float v) {
    if (v <= 9.0f) return Severity::Good;
    if (v <= 35.4f) return Severity::Moderate;
    if (v <= 55.4f) return Severity::Poor;
    if (v <= 125.4f) return Severity::Bad;
    return Severity::Severe;
}
static Severity sevCo2(float v) {
    if (v < 800) return Severity::Good;
    if (v < 1000) return Severity::Moderate;
    if (v < 1500) return Severity::Poor;
    if (v < 2000) return Severity::Bad;
    return Severity::Severe;
}
static Severity sevAqi(float v) {
    if (v <= 50) return Severity::Good;
    if (v <= 100) return Severity::Moderate;
    if (v <= 150) return Severity::Poor;
    if (v <= 200) return Severity::Bad;
    return Severity::Severe;
}
static Severity sevTvoc(float v) {
    if (v <= 150) return Severity::Good;
    if (v <= 250) return Severity::Moderate;
    if (v <= 400) return Severity::Poor;
    return Severity::Bad;
}
static Severity sevNox(float v) {
    if (v <= 1) return Severity::Good;
    if (v <= 20) return Severity::Moderate;
    if (v <= 100) return Severity::Poor;
    return Severity::Bad;
}
static Severity sevNeutral(float) { return Severity::Neutral; }

// ---- descriptor table ----------------------------------------------------
static const MetricDescriptor DESCRIPTORS[] = {
    {"pm25", "PM2.5", "ug/m3", 1, [](const AirGradientReading& r) { return r.pm25; }, sevPm25},
    {"co2", "CO2", "ppm", 0, [](const AirGradientReading& r) { return r.co2; }, sevCo2},
    {"aqi", "US AQI", "", 0, [](const AirGradientReading& r) { return r.aqi; }, sevAqi},
    {"temp", "Temperature", "\xC2\xB0""C", 1, [](const AirGradientReading& r) { return r.temperature; }, sevNeutral},
    {"humidity", "Humidity", "%", 0, [](const AirGradientReading& r) { return r.humidity; }, sevNeutral},
    {"tvoc", "TVOC index", "", 0, [](const AirGradientReading& r) { return r.tvoc; }, sevTvoc},
    {"nox", "NOx index", "", 0, [](const AirGradientReading& r) { return r.nox; }, sevNox},
};

const MetricDescriptor* MetricWidget::find(const char* id) {
    for (const auto& d : DESCRIPTORS) {
        if (strcmp(d.id, id) == 0) return &d;
    }
    return nullptr;
}

// ---- rendering -----------------------------------------------------------
void MetricWidget::create(lv_obj_t* parent, const ThemeManager& theme) {
    const ThemePalette& p = theme.palette();
    _root = makeCard(parent, p);

    _bar = lv_obj_create(_root);
    lv_obj_remove_style_all(_bar);
    lv_obj_set_style_bg_opa(_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_bar, 3, 0);
    lv_obj_set_size(_bar, 6, LV_PCT(100));
    lv_obj_align(_bar, LV_ALIGN_LEFT_MID, -6, 0);

    _nameLbl = lv_label_create(_root);
    lv_obj_set_style_text_font(_nameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_nameLbl, p.textMuted, 0);
    lv_label_set_text(_nameLbl, _desc->label);
    lv_obj_align(_nameLbl, LV_ALIGN_TOP_LEFT, 8, 0);

    _valueLbl = lv_label_create(_root);
    lv_obj_set_style_text_font(_valueLbl, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(_valueLbl, p.text, 0);
    lv_label_set_text(_valueLbl, "--");
    lv_obj_align(_valueLbl, LV_ALIGN_LEFT_MID, 8, 10);

    _unitLbl = lv_label_create(_root);
    lv_obj_set_style_text_font(_unitLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_unitLbl, p.textMuted, 0);
    lv_label_set_text(_unitLbl, _desc->unit);
    lv_obj_align(_unitLbl, LV_ALIGN_BOTTOM_LEFT, 8, 0);
}

void MetricWidget::update(const AirGradientReading& r, const AppSettings& s,
                          const ThemeManager& theme) {
    float v = r.valid ? _desc->get(r) : NAN;

    // Unit conversions driven by settings.
    const char* unit = _desc->unit;
    if (strcmp(_desc->id, "temp") == 0 && s.tempFahrenheit) {
        if (!isnan(v)) v = v * 9.0f / 5.0f + 32.0f;
        unit = "\xC2\xB0""F";
    }
    lv_label_set_text(_unitLbl, unit);

    if (isnan(v)) {
        lv_label_set_text(_valueLbl, "--");
        lv_obj_set_style_bg_color(_bar, theme.palette().cardBorder, 0);
        return;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%.*f", _desc->decimals, v);
    lv_label_set_text(_valueLbl, buf);
    lv_obj_set_style_bg_color(_bar, theme.severityColor(_desc->severity(v)), 0);
}
