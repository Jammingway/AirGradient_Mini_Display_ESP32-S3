#include "Dashboard.h"
#include <ArduinoJson.h>
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

    buildTopBar(theme);

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

    _titleLbl = lv_label_create(_screen);
    lv_obj_set_style_text_font(_titleLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_titleLbl, p.text, 0);
    lv_label_set_text(_titleLbl, "AirGradient");
    lv_obj_align(_titleLbl, LV_ALIGN_TOP_LEFT, GAP + 4, 16);

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

        // Tapping the "last updated" card triggers a manual refresh.
        InfoWidget* info = w->asInfo();
        if (info && info->kind() == InfoWidget::Kind::Updated) {
            lv_obj_t* card = info->root();
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
            // Visible press feedback so a registered tap is unmistakable.
            lv_obj_set_style_border_color(card, theme.palette().accent, LV_STATE_PRESSED);
            lv_obj_set_style_border_width(card, 2, LV_STATE_PRESSED);
            lv_obj_add_event_cb(card, [](lv_event_t* e) {
                static_cast<Dashboard*>(lv_event_get_user_data(e))->onUpdatedCardClicked();
            }, LV_EVENT_SHORT_CLICKED, this);
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
    for (auto& w : _widgets) w->update(r, s, theme);
    if (r.valid && r.deviceName.length()) {
        lv_label_set_text(_titleLbl, r.deviceName.c_str());
    }
}

void Dashboard::updateStatus(const String& wifiText, bool wifiAlert,
                             const String& updatedText, bool updatedAlert,
                             const String& headline, const ThemeManager& theme) {
    for (auto& w : _widgets) {
        InfoWidget* info = w->asInfo();
        if (!info) continue;
        if (info->kind() == InfoWidget::Kind::Wifi) info->setText(wifiText, wifiAlert, theme);
        else info->setText(updatedText, updatedAlert, theme);
    }
    lv_label_set_text(_headlineLbl, headline.c_str());
}
