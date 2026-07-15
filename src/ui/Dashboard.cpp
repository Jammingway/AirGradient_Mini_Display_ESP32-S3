#include "Dashboard.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>
#include "board_pins.h"
#include "../themes/ThemeManager.h"
#include "../settings/SettingsManager.h"
#include "../widgets/MetricWidget.h"
#include "../utils/Logger.h"

void Dashboard::create(const ThemeManager& theme, const SettingsManager& settings) {
    _theme = &theme;
    const ThemePalette& p = theme.palette();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, p.bg, 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _customName = settings.get().deviceName;
    buildTopBar(theme);
    applyName();

    _grid = lv_obj_create(_screen);
    lv_obj_remove_style_all(_grid);
    lv_obj_set_pos(_grid, GAP, TOP_BAR_H);
    lv_obj_set_size(_grid, LCD_H_RES - 2 * GAP, LCD_V_RES - TOP_BAR_H - GAP);
    lv_obj_remove_flag(_grid, LV_OBJ_FLAG_SCROLLABLE);

    buildWidgets(theme, settings);
}

void Dashboard::load(bool deletePrev) {
    lv_screen_load_anim(_screen, LV_SCR_LOAD_ANIM_FADE_IN, 350, 0, deletePrev);
}

void Dashboard::setDebugLine(const String& text) {
    if (!_debugLbl) {
        _debugLbl = lv_label_create(_screen);
        lv_obj_set_style_text_font(_debugLbl, &lv_font_unscii_16, 0);
        lv_obj_set_style_text_color(_debugLbl, lv_color_hex(0x33FF66), 0);
        lv_obj_set_style_bg_color(_debugLbl, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(_debugLbl, LV_OPA_60, 0);
        lv_obj_set_style_pad_all(_debugLbl, 2, 0);
        lv_label_set_long_mode(_debugLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(_debugLbl, LCD_H_RES - 8);
        lv_obj_align(_debugLbl, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    }
    if (text.length() == 0) {
        lv_obj_add_flag(_debugLbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(_debugLbl, text.c_str());
    lv_obj_move_foreground(_debugLbl);
    lv_obj_remove_flag(_debugLbl, LV_OBJ_FLAG_HIDDEN);
}

void Dashboard::rebuild(const ThemeManager& theme, const SettingsManager& settings) {
    _theme = &theme;
    _widgets.clear();
    lv_obj_clean(_grid);
    buildWidgets(theme, settings);
}

void Dashboard::buildTopBar(const ThemeManager& theme) {
    const ThemePalette& p = theme.palette();

    // The sensor name sits in a rounded outline. It is display-only; the name
    // itself is edited on the settings General tab.
    _titleBox = lv_obj_create(_screen);
    lv_obj_remove_style_all(_titleBox);
    lv_obj_set_size(_titleBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(_titleBox, 10, 0);
    lv_obj_set_style_border_width(_titleBox, 1, 0);
    lv_obj_set_style_border_color(_titleBox, lv_color_white(), 0);
    lv_obj_set_style_pad_hor(_titleBox, 10, 0);
    lv_obj_set_style_pad_ver(_titleBox, 5, 0);
    lv_obj_align(_titleBox, LV_ALIGN_TOP_LEFT, GAP + 4, 10);
    lv_obj_remove_flag(_titleBox, LV_OBJ_FLAG_SCROLLABLE);

    _titleLbl = lv_label_create(_titleBox);
    lv_obj_set_style_text_font(_titleLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_titleLbl, p.text, 0);
    lv_label_set_text(_titleLbl, "AirGradient");

    _headlineLbl = lv_label_create(_screen);
    lv_obj_set_style_text_font(_headlineLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_headlineLbl, p.textMuted, 0);
    lv_label_set_text(_headlineLbl, "");
    lv_obj_align(_headlineLbl, LV_ALIGN_TOP_RIGHT, -(2 * 56 + GAP + 12), 20);

    auto makeButton = [&](const char* symbol, int slot, void (*handler)(lv_event_t*)) {
        lv_obj_t* btn = lv_button_create(_screen);
        lv_obj_set_size(btn, 44, 36);
        lv_obj_set_style_bg_color(btn, p.card, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, p.cardBorder, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -GAP - slot * 56, 10);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, symbol);
        lv_obj_set_style_text_color(lbl, p.text, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, handler, LV_EVENT_CLICKED, this);
        return btn;
    };

    makeButton(LV_SYMBOL_SETTINGS, 0, [](lv_event_t* e) {
        auto* self = static_cast<Dashboard*>(lv_event_get_user_data(e));
        if (self->_settingsCb) self->_settingsCb();
    });
    makeButton(LV_SYMBOL_REFRESH, 1, [](lv_event_t* e) {
        auto* self = static_cast<Dashboard*>(lv_event_get_user_data(e));
        if (self->_refreshCb) self->_refreshCb();
    });
}

void Dashboard::buildWidgets(const ThemeManager& theme, const SettingsManager& settings) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, settings.get().layoutJson);
    if (err || !doc["layout"].is<JsonArray>()) {
        LOG_W("dashboard", "layout JSON invalid (%s), using default", err.c_str());
        deserializeJson(doc, SettingsManager::defaultLayoutJson());
    }
    JsonArray layout = doc["layout"].as<JsonArray>();

    // Instantiate the enabled widgets in layout order; positions are ignored
    // here and computed by flowLayout() so the grid always fills the screen.
    for (JsonObject item : layout) {
        const char* type = item["type"] | "";
        std::unique_ptr<Widget> w;
        if (strcmp(type, "metric") == 0) {
            const MetricDescriptor* desc = MetricWidget::find(item["metric"] | "");
            if (!desc) {
                LOG_W("dashboard", "unknown metric '%s' skipped", item["metric"] | "");
                continue;
            }
            w.reset(new MetricWidget(desc));
        } else if (strcmp(type, "wifi") == 0) {
            w.reset(new InfoWidget(InfoWidget::Kind::Wifi));
        } else if (strcmp(type, "updated") == 0) {
            w.reset(new InfoWidget(InfoWidget::Kind::Updated));
        } else {
            continue;
        }

        w->create(_grid, theme);

        InfoWidget* info = w->asInfo();
        if (info && info->kind() == InfoWidget::Kind::Updated) {
            // Tapping the "last updated" card triggers a manual refresh.
            lv_obj_t* card = info->root();
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_border_color(card, theme.palette().accent, LV_STATE_PRESSED);
            lv_obj_set_style_border_width(card, 2, LV_STATE_PRESSED);
            lv_obj_add_event_cb(card, [](lv_event_t* e) {
                static_cast<Dashboard*>(lv_event_get_user_data(e))->onUpdatedCardClicked();
            }, LV_EVENT_SHORT_CLICKED, this);
        } else if (!info) {
            // Metric card: tap to open its full-screen trend chart.
            int me = metricEnumForId(item["metric"] | "");
            if (me >= 0) {
                lv_obj_t* card = w->root();
                lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_user_data(card, (void*)(intptr_t)me);
                lv_obj_set_style_border_color(card, theme.palette().accent, LV_STATE_PRESSED);
                lv_obj_set_style_border_width(card, 2, LV_STATE_PRESSED);
                lv_obj_add_event_cb(card, [](lv_event_t* e) {
                    auto* self = static_cast<Dashboard*>(lv_event_get_user_data(e));
                    lv_obj_t* c = (lv_obj_t*)lv_event_get_current_target(e);
                    self->showChart((int)(intptr_t)lv_obj_get_user_data(c));
                }, LV_EVENT_SHORT_CLICKED, this);
            }
        }
        _widgets.push_back(std::move(w));
    }

    flowLayout();
}

void Dashboard::flowLayout() {
    // Pack the enabled widgets into a grid that fills the whole area as
    // evenly as possible: choose the row count whose cells sit closest to a
    // pleasant aspect ratio, spread widgets across rows, and let shorter rows
    // stretch their cells wider so there are never gaps (horizontally or
    // vertically). Disabling a widget simply reflows the rest to fill the
    // freed space.
    int n = (int)_widgets.size();
    if (n == 0) return;

    int gridW = LCD_H_RES - 2 * GAP;
    int gridH = LCD_V_RES - TOP_BAR_H - GAP;

    int rows = 1;
    float bestScore = 1e9f;
    for (int tryRows = 1; tryRows <= min(n, 5); tryRows++) {
        int cols = (n + tryRows - 1) / tryRows;
        float cellAspect = ((float)gridW / cols) / ((float)gridH / tryRows);
        float score = fabsf(cellAspect - 1.7f);  // target: slightly wide cards
        if (score < bestScore) {
            bestScore = score;
            rows = tryRows;
        }
    }

    int base = n / rows;
    int extra = n % rows;  // the first `extra` rows carry one more widget
    int cellH = (gridH - (rows - 1) * GAP) / rows;

    int idx = 0;
    for (int r = 0; r < rows; r++) {
        int count = base + (r < extra ? 1 : 0);
        if (count <= 0) continue;
        int cellW = (gridW - (count - 1) * GAP) / count;
        for (int c = 0; c < count; c++) {
            _widgets[idx++]->setRect(c * (cellW + GAP), r * (cellH + GAP), cellW, cellH);
        }
    }
    LOG_I("dashboard", "flowed %d widgets into %d rows", n, rows);
}

void Dashboard::onUpdatedCardClicked() {
    // Immediate feedback; the next status tick overwrites it with real age.
    for (auto& w : _widgets) {
        InfoWidget* info = w->asInfo();
        if (info && info->kind() == InfoWidget::Kind::Updated && _theme) {
            info->setText("refreshing...", false, *_theme);
        }
    }
    if (_refreshCb) _refreshCb();
}

void Dashboard::updateReading(const AirGradientReading& r, const AppSettings& s,
                              const ThemeManager& theme) {
    _tempF = s.tempFahrenheit;
    for (auto& w : _widgets) w->update(r, s, theme);
    if (r.valid && r.deviceName.length()) _reportedName = r.deviceName;
    _customName = s.deviceName;
    applyName();
    // Live-update a chart that happens to be open.
    if (_chartOverlay && !lv_obj_has_flag(_chartOverlay, LV_OBJ_FLAG_HIDDEN)) {
        refreshChart();
    }
}

// ---------------------- sensor name ----------------------

void Dashboard::applyName() {
    if (!_titleLbl) return;
    String name = _customName.length() ? _customName : _reportedName;
    if (!name.length()) name = "AirGradient";
    lv_label_set_text(_titleLbl, name.c_str());
}

void Dashboard::setCustomName(const String& name) {
    _customName = name;
    applyName();
}

// ---------------------- trend chart overlay ----------------------

int Dashboard::metricEnumForId(const char* id) {
    if (!strcmp(id, "pm25")) return (int)Metric::Pm25;
    if (!strcmp(id, "co2")) return (int)Metric::Co2;
    if (!strcmp(id, "temp")) return (int)Metric::Temp;
    if (!strcmp(id, "humidity")) return (int)Metric::Humidity;
    if (!strcmp(id, "tvoc")) return (int)Metric::Tvoc;
    if (!strcmp(id, "nox")) return (int)Metric::Nox;
    if (!strcmp(id, "aqi")) return (int)Metric::Aqi;
    return -1;
}

struct ChartMeta { const char* name; const char* unit; uint8_t decimals; };
static const ChartMeta CHART_META[(int)Metric::Count] = {
    {"PM2.5", "ug/m3", 1}, {"CO2", "ppm", 0}, {"Temperature", "C", 1},
    {"Humidity", "%", 0}, {"TVOC", "index", 0}, {"NOx", "index", 0},
    {"US AQI", "", 0},
};

static const struct { const char* lbl; uint16_t min; } DURATIONS[4] = {
    {"1h", 60}, {"6h", 360}, {"24h", 1440}, {"All", 0},
};

void Dashboard::buildChartOverlay() {
    if (_chartOverlay || !_theme) return;
    const ThemePalette& p = _theme->palette();

    _chartOverlay = lv_obj_create(_screen);
    lv_obj_remove_style_all(_chartOverlay);
    lv_obj_set_size(_chartOverlay, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(_chartOverlay, 0, 0);
    lv_obj_set_style_bg_color(_chartOverlay, p.bg, 0);
    lv_obj_set_style_bg_opa(_chartOverlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(_chartOverlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_chartOverlay, LV_OBJ_FLAG_SCROLLABLE);
    // Tap anywhere that isn't a duration button closes the chart.
    lv_obj_add_event_cb(_chartOverlay, [](lv_event_t* e) {
        static_cast<Dashboard*>(lv_event_get_user_data(e))->hideChart();
    }, LV_EVENT_CLICKED, this);

    _chartTitle = lv_label_create(_chartOverlay);
    lv_obj_set_style_text_font(_chartTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_chartTitle, p.text, 0);
    lv_obj_align(_chartTitle, LV_ALIGN_TOP_LEFT, 16, 12);

    lv_obj_t* hint = lv_label_create(_chartOverlay);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, p.textMuted, 0);
    lv_label_set_text(hint, "tap chart to close");
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -16, 18);

    _chartStats = lv_label_create(_chartOverlay);
    lv_obj_set_style_text_font(_chartStats, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_chartStats, p.textMuted, 0);
    lv_obj_align(_chartStats, LV_ALIGN_TOP_LEFT, 16, 46);

    _chart = lv_chart_create(_chartOverlay);
    lv_obj_set_size(_chart, LCD_H_RES - 48, LCD_V_RES - 148);
    lv_obj_align(_chart, LV_ALIGN_TOP_MID, 0, 78);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(_chart, 5, 6);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_color(_chart, p.card, 0);
    lv_obj_set_style_border_color(_chart, p.cardBorder, 0);
    lv_obj_set_style_border_width(_chart, 1, 0);
    lv_obj_set_style_line_color(_chart, p.cardBorder, LV_PART_MAIN);  // div lines
    lv_obj_set_style_size(_chart, 0, 0, LV_PART_INDICATOR);           // no point dots
    // Taps on the chart fall through to the overlay (which closes it).
    lv_obj_remove_flag(_chart, LV_OBJ_FLAG_CLICKABLE);
    _chartSeries = lv_chart_add_series(_chart, p.accent, LV_CHART_AXIS_PRIMARY_Y);

    // Duration buttons along the bottom.
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = lv_button_create(_chartOverlay);
        lv_obj_set_size(b, 96, 44);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, (i - 2) * 108 + 54, -10);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_user_data(b, (void*)(intptr_t)DURATIONS[i].min);
        lv_obj_t* lbl = lv_label_create(b);
        lv_label_set_text(lbl, DURATIONS[i].lbl);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(b, [](lv_event_t* e) {
            auto* self = static_cast<Dashboard*>(lv_event_get_user_data(e));
            lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
            self->_chartWindowMin = (uint16_t)(intptr_t)lv_obj_get_user_data(btn);
            self->highlightDurationButtons();
            self->refreshChart();
        }, LV_EVENT_CLICKED, this);
        _durBtns[i] = b;
    }
}

void Dashboard::highlightDurationButtons() {
    if (!_theme) return;
    const ThemePalette& p = _theme->palette();
    for (int i = 0; i < 4; i++) {
        if (!_durBtns[i]) continue;
        bool active = DURATIONS[i].min == _chartWindowMin;
        lv_obj_set_style_bg_color(_durBtns[i], active ? p.accent : p.card, 0);
        lv_obj_set_style_border_width(_durBtns[i], 1, 0);
        lv_obj_set_style_border_color(_durBtns[i], p.cardBorder, 0);
    }
}

void Dashboard::showChart(int metric) {
    if (!_history || metric < 0 || metric >= (int)Metric::Count) return;
    buildChartOverlay();
    _chartMetric = metric;
    highlightDurationButtons();
    refreshChart();
    lv_obj_remove_flag(_chartOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_chartOverlay);
}

void Dashboard::hideChart() {
    if (_chartOverlay) lv_obj_add_flag(_chartOverlay, LV_OBJ_FLAG_HIDDEN);
}

void Dashboard::refreshChart() {
    if (!_chart || _chartMetric < 0 || !_history) return;

    static float vals[100];
    static uint32_t ages[100];
    uint32_t nowMs = millis();
    uint32_t durMs = (uint32_t)_chartWindowMin * 60000UL;
    int n = _history->query((Metric)_chartMetric, durMs, nowMs, 100, vals, ages);

    const ChartMeta& cm = CHART_META[_chartMetric];
    bool isTemp = ((Metric)_chartMetric == Metric::Temp);
    bool asF = isTemp && _tempF;
    auto conv = [&](float v) { return asF ? v * 9.0f / 5.0f + 32.0f : v; };

    // Stats over the non-NaN samples.
    float mn = 1e9f, mx = -1e9f, sum = 0;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (isnan(vals[i])) continue;
        float v = conv(vals[i]);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        cnt++;
    }

    // Plot the line (rounded to int; the cards show precise values).
    lv_chart_set_point_count(_chart, n > 0 ? n : 1);
    if (cnt > 0) {
        float pad = (mx - mn) * 0.1f;
        if (pad < 1.0f) pad = 1.0f;
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y,
                           (int32_t)floorf(mn - pad), (int32_t)ceilf(mx + pad));
    } else {
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
    }
    for (int i = 0; i < n; i++) {
        int32_t y = isnan(vals[i]) ? LV_CHART_POINT_NONE : (int32_t)lroundf(conv(vals[i]));
        lv_chart_set_value_by_id(_chart, _chartSeries, i, y);
    }
    lv_chart_refresh(_chart);

    // Title: "PM2.5 (ug/m3)".
    const char* unit = asF ? "F" : cm.unit;
    char title[48];
    if (unit && unit[0]) snprintf(title, sizeof(title), "%s (%s)", cm.name, unit);
    else                 snprintf(title, sizeof(title), "%s", cm.name);
    lv_label_set_text(_chartTitle, title);

    // Stats line.
    uint32_t spanMin = _history->oldestAgeMs(nowMs) / 60000UL;
    char stats[128];
    if (cnt > 0) {
        snprintf(stats, sizeof(stats),
                 "min %.*f   max %.*f   avg %.*f      %s   %d pts",
                 cm.decimals, mn, cm.decimals, mx, cm.decimals, sum / cnt,
                 _chartWindowMin ? DURATIONS[_chartWindowMin == 60 ? 0 : _chartWindowMin == 360 ? 1 : 2].lbl
                                 : "all",
                 cnt);
    } else {
        snprintf(stats, sizeof(stats), "no samples yet in this window (history spans %luh)",
                 (unsigned long)(spanMin / 60));
    }
    lv_label_set_text(_chartStats, stats);
}

void Dashboard::updateStatus(const DashboardStatus& st, const ThemeManager& theme) {
    for (auto& w : _widgets) {
        InfoWidget* info = w->asInfo();
        if (!info) continue;
        if (info->kind() == InfoWidget::Kind::Wifi) {
            info->setText(st.wifiText, st.wifiAlert, theme);
            info->setRow(InfoWidget::WifiIp, st.localIp);
            info->setRow(InfoWidget::WifiSensor, st.sensorTarget);
        } else {
            info->setText(st.updatedText, st.updatedAlert, theme);
            info->setRow(InfoWidget::SysUptime, st.uptime);
            info->setRow(InfoWidget::SysSensorUptime, st.sensorUptime);
        }
    }
    lv_label_set_text(_headlineLbl, st.headline.c_str());
}
