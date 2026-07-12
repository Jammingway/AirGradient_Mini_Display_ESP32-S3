#include "SettingsScreen.h"
#include <ArduinoJson.h>
#include "board_pins.h"
#include "../settings/SettingsManager.h"
#include "../themes/ThemeManager.h"
#include "../utils/Logger.h"

// Widget catalog for the Dashboard tab; order matches _swWidgets.
struct WidgetEntry { const char* id; const char* type; const char* label; };
static const WidgetEntry WIDGET_CATALOG[] = {
    {"pm25", "metric", "PM2.5"},
    {"co2", "metric", "CO2"},
    {"aqi", "metric", "US AQI"},
    {"temp", "metric", "Temperature"},
    {"humidity", "metric", "Humidity"},
    {"tvoc", "metric", "TVOC index"},
    {"nox", "metric", "NOx index"},
    {"wifi", "wifi", "WiFi status"},
    {"updated", "updated", "Last updated"},
};

static const char* POLL_OPTIONS = "2 min\n5 min\n10 min\n15 min";
static const uint16_t POLL_VALUES[] = {120, 300, 600, 900};
static constexpr uint16_t POLL_DEFAULT_INDEX = 1;  // 5 min
static const char* TIMEOUT_OPTIONS = "5 s\n10 s\n30 s";
static const uint16_t TIMEOUT_VALUES[] = {5, 10, 30};
static const char* RETRY_COUNT_OPTIONS = "1 (no retry)\n2\n3\n5\n10";
static const uint16_t RETRY_COUNT_VALUES[] = {1, 2, 3, 5, 10};
static constexpr uint16_t RETRY_COUNT_DEFAULT_INDEX = 2;  // 3 attempts
static const char* RETRY_DELAY_OPTIONS = "5 s\n10 s\n30 s\n60 s";
static const uint16_t RETRY_DELAY_VALUES[] = {5, 10, 30, 60};
static constexpr uint16_t RETRY_DELAY_DEFAULT_INDEX = 1;  // 10 s
static const char* SLEEP_OPTIONS = "Never\n3 min\n5 min\n15 min\n30 min\n60 min";
static const uint16_t SLEEP_VALUES[] = {0, 3, 5, 15, 30, 60};

static uint16_t indexOfValue(const uint16_t* values, size_t n, uint16_t v,
                             uint16_t fallback = 0) {
    for (size_t i = 0; i < n; i++) {
        if (values[i] == v) return i;
    }
    return fallback;
}

void SettingsScreen::open(SettingsManager& settings, const ThemeManager& theme, CloseCallback cb) {
    _settings = &settings;
    _theme = &theme;
    _closeCb = std::move(cb);
    _result = {};
    const ThemePalette& p = theme.palette();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, p.bg, 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Header: back button + title
    lv_obj_t* back = lv_button_create(_screen);
    lv_obj_set_size(back, 76, 40);
    lv_obj_set_style_bg_color(back, p.card, 0);
    lv_obj_set_style_border_color(back, p.cardBorder, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_obj_t* backLbl = lv_label_create(back);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Save");
    lv_obj_set_style_text_color(backLbl, p.text, 0);
    lv_obj_center(backLbl);
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        static_cast<SettingsScreen*>(lv_event_get_user_data(e))->close();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* title = lv_label_create(_screen);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, p.text, 0);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // Tabs
    lv_obj_t* tabs = lv_tabview_create(_screen);
    lv_tabview_set_tab_bar_size(tabs, 44);
    lv_obj_set_pos(tabs, 0, 56);
    lv_obj_set_size(tabs, LCD_H_RES, LCD_V_RES - 56);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_TRANSP, 0);
    lv_obj_t* bar = lv_tabview_get_tab_bar(tabs);
    lv_obj_set_style_bg_color(bar, p.card, 0);
    lv_obj_set_style_text_color(bar, p.text, 0);

    buildNetworkTab(lv_tabview_add_tab(tabs, "Network"));
    buildApiTab(lv_tabview_add_tab(tabs, "API"));
    buildGeneralTab(lv_tabview_add_tab(tabs, "General"));
    buildDashboardTab(lv_tabview_add_tab(tabs, "Dashboard"));

    // Shared keyboard, hidden until a textarea takes focus.
    _keyboard = lv_keyboard_create(_screen);
    lv_obj_set_size(_keyboard, LCD_H_RES, 200);
    lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(self->_keyboard, nullptr);
    }, LV_EVENT_READY, this);
    lv_obj_add_event_cb(_keyboard, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(self->_keyboard, nullptr);
    }, LV_EVENT_CANCEL, this);

    lv_screen_load_anim(_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

// ---------- row builders ----------------------------------------------------

static lv_obj_t* makeRowBase(lv_obj_t* parent, const char* label, const ThemePalette& p,
                             int height = 56) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), height);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, p.textMuted, 0);
    lv_label_set_text(lbl, label);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);
    return row;
}

lv_obj_t* SettingsScreen::makeTextRow(lv_obj_t* parent, const char* label,
                                      const String& value, bool password) {
    const ThemePalette& p = _theme->palette();
    lv_obj_t* row = makeRowBase(parent, label, p, 68);

    lv_obj_t* ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, value.c_str());
    lv_textarea_set_password_mode(ta, password);
    lv_obj_set_size(ta, 470, 56);
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_pad_ver(ta, 14, 0);
    lv_obj_set_style_bg_color(ta, p.card, 0);
    lv_obj_set_style_text_color(ta, p.text, 0);
    lv_obj_set_style_border_color(ta, p.cardBorder, 0);
    // Generous touch slop so fingers don't need pixel accuracy.
    lv_obj_set_ext_click_area(ta, 10);
    attachKeyboard(ta);
    return ta;
}

lv_obj_t* SettingsScreen::makeDropdownRow(lv_obj_t* parent, const char* label,
                                          const char* options, uint16_t selected) {
    const ThemePalette& p = _theme->palette();
    lv_obj_t* row = makeRowBase(parent, label, p);

    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_dropdown_set_selected(dd, selected);
    lv_obj_set_width(dd, 220);
    lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(dd, p.card, 0);
    lv_obj_set_style_text_color(dd, p.text, 0);
    lv_obj_set_style_border_color(dd, p.cardBorder, 0);
    return dd;
}

lv_obj_t* SettingsScreen::makeSwitchRow(lv_obj_t* parent, const char* label, bool state) {
    const ThemePalette& p = _theme->palette();
    lv_obj_t* row = makeRowBase(parent, label, p);

    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    if (state) lv_obj_add_state(sw, LV_STATE_CHECKED);
    return sw;
}

void SettingsScreen::attachKeyboard(lv_obj_t* textarea) {
    lv_obj_add_event_cb(textarea, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        lv_keyboard_set_textarea(self->_keyboard, (lv_obj_t*)lv_event_get_target(e));
        lv_obj_remove_flag(self->_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(self->_keyboard);
    }, LV_EVENT_FOCUSED, this);
}

// ---------- tabs -------------------------------------------------------------

void SettingsScreen::buildNetworkTab(lv_obj_t* tab) {
    const AppSettings& s = _settings->get();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    _taSsid1 = makeTextRow(tab, "Primary SSID", s.ssid1);
    _taPass1 = makeTextRow(tab, "Primary password", s.pass1, true);
    _taSsid2 = makeTextRow(tab, "Fallback SSID (optional)", s.ssid2);
    _taPass2 = makeTextRow(tab, "Fallback password", s.pass2, true);
}

void SettingsScreen::buildApiTab(lv_obj_t* tab) {
    const AppSettings& s = _settings->get();
    const ThemePalette& p = _theme->palette();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    _taEndpoint = makeTextRow(tab, "Endpoint URL", s.endpoint);

    // API token: wide field + eye button that temporarily reveals the value.
    _taApiKey = makeTextRow(tab, "API token", s.apiKey, true);
    lv_obj_t* row = lv_obj_get_parent(_taApiKey);
    lv_obj_set_width(_taApiKey, 470 - 64);
    lv_obj_align(_taApiKey, LV_ALIGN_RIGHT_MID, -68, 0);

    lv_obj_t* eye = lv_button_create(row);
    lv_obj_set_size(eye, 56, 56);
    lv_obj_set_style_bg_color(eye, p.card, 0);
    lv_obj_set_style_border_width(eye, 1, 0);
    lv_obj_set_style_border_color(eye, p.cardBorder, 0);
    lv_obj_set_style_shadow_width(eye, 0, 0);
    lv_obj_align(eye, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_t* eyeLbl = lv_label_create(eye);
    lv_label_set_text(eyeLbl, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(eyeLbl, p.text, 0);
    lv_obj_center(eyeLbl);
    lv_obj_add_event_cb(eye, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        bool hidden = lv_textarea_get_password_mode(self->_taApiKey);
        lv_textarea_set_password_mode(self->_taApiKey, !hidden);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
        lv_label_set_text(lv_obj_get_child(btn, 0),
                          hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }, LV_EVENT_CLICKED, this);

    _ddPoll = makeDropdownRow(tab, "Polling interval", POLL_OPTIONS,
                              indexOfValue(POLL_VALUES, 4, s.pollIntervalSec,
                                           POLL_DEFAULT_INDEX));
    _ddTimeout = makeDropdownRow(tab, "Request timeout", TIMEOUT_OPTIONS,
                                 indexOfValue(TIMEOUT_VALUES, 3, s.timeoutSec));
    _ddRetryCount = makeDropdownRow(tab, "Attempts per poll", RETRY_COUNT_OPTIONS,
                                    indexOfValue(RETRY_COUNT_VALUES, 5, s.retryCount,
                                                 RETRY_COUNT_DEFAULT_INDEX));
    _ddRetryDelay = makeDropdownRow(tab, "Delay between attempts", RETRY_DELAY_OPTIONS,
                                    indexOfValue(RETRY_DELAY_VALUES, 4, s.retryDelaySec,
                                                 RETRY_DELAY_DEFAULT_INDEX));
}

void SettingsScreen::buildGeneralTab(lv_obj_t* tab) {
    const AppSettings& s = _settings->get();
    const ThemePalette& p = _theme->palette();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);

    _ddTheme = makeDropdownRow(tab, "Theme", "Dark\nLight", s.theme == "light" ? 1 : 0);

    lv_obj_t* row = makeRowBase(tab, "Brightness", p);
    _slBrightness = lv_slider_create(row);
    lv_slider_set_range(_slBrightness, 20, 100);
    lv_slider_set_value(_slBrightness, s.brightness, LV_ANIM_OFF);
    lv_obj_set_size(_slBrightness, 320, 12);
    lv_obj_align(_slBrightness, LV_ALIGN_RIGHT_MID, -16, 0);

    _ddSleep = makeDropdownRow(tab, "Sleep after", SLEEP_OPTIONS,
                               indexOfValue(SLEEP_VALUES, 6, s.sleepTimeoutMin));
    _swTempF = makeSwitchRow(tab, "Temperature in \xC2\xB0""F", s.tempFahrenheit);
}

void SettingsScreen::buildDashboardTab(lv_obj_t* tab) {
    const ThemePalette& p = _theme->palette();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);

    // Which widgets are currently in the layout?
    JsonDocument doc;
    deserializeJson(doc, _settings->get().layoutJson);
    auto enabled = [&](const WidgetEntry& entry) {
        for (JsonObject item : doc["layout"].as<JsonArray>()) {
            const char* type = item["type"] | "";
            if (strcmp(type, entry.type) != 0) continue;
            if (strcmp(entry.type, "metric") == 0) {
                if (strcmp(item["metric"] | "", entry.id) == 0) return true;
            } else {
                return true;
            }
        }
        return false;
    };

    for (int i = 0; i < WIDGET_COUNT; i++) {
        _swWidgets[i] = makeSwitchRow(tab, WIDGET_CATALOG[i].label, enabled(WIDGET_CATALOG[i]));
    }

    lv_obj_t* resetBtn = lv_button_create(tab);
    lv_obj_set_size(resetBtn, LV_PCT(100), 48);
    lv_obj_set_style_bg_color(resetBtn, p.card, 0);
    lv_obj_t* resetLbl = lv_label_create(resetBtn);
    lv_label_set_text(resetLbl, "Reset layout to default");
    lv_obj_set_style_text_color(resetLbl, p.text, 0);
    lv_obj_center(resetLbl);
    lv_obj_add_event_cb(resetBtn, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        for (int i = 0; i < WIDGET_COUNT; i++) {
            lv_obj_add_state(self->_swWidgets[i], LV_STATE_CHECKED);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* factoryBtn = lv_button_create(tab);
    lv_obj_set_size(factoryBtn, LV_PCT(100), 48);
    lv_obj_set_style_bg_color(factoryBtn, lv_color_hex(0x8B2E2E), 0);
    lv_obj_t* factoryLbl = lv_label_create(factoryBtn);
    lv_label_set_text(factoryLbl, "Factory reset (erase all settings)");
    lv_obj_set_style_text_color(factoryLbl, lv_color_white(), 0);
    lv_obj_center(factoryLbl);
    lv_obj_add_event_cb(factoryBtn, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        self->_settings->factoryReset();
        self->_result.factoryReset = true;
        self->_result.networkChanged = true;
        self->_result.apiChanged = true;
        self->_result.generalChanged = true;
        self->_result.layoutChanged = true;
        // Skip save(): the freshly reset values must survive.
        auto cb = self->_closeCb;
        Result res = self->_result;
        self->_screen = nullptr;
        // The callback loads the next screen with auto-delete of this one;
        // deleting it here while it is still the active screen hangs LVGL.
        if (cb) cb(res);
    }, LV_EVENT_CLICKED, this);
}

// ---------- save / close -----------------------------------------------------

void SettingsScreen::save() {
    const AppSettings& s = _settings->get();

    String ssid1 = lv_textarea_get_text(_taSsid1);
    String pass1 = lv_textarea_get_text(_taPass1);
    String ssid2 = lv_textarea_get_text(_taSsid2);
    String pass2 = lv_textarea_get_text(_taPass2);
    if (ssid1 != s.ssid1 || pass1 != s.pass1 || ssid2 != s.ssid2 || pass2 != s.pass2) {
        _settings->setNetwork(ssid1, pass1, ssid2, pass2);
        _result.networkChanged = true;
    }

    String endpoint = lv_textarea_get_text(_taEndpoint);
    String apiKey = lv_textarea_get_text(_taApiKey);
    uint16_t poll = POLL_VALUES[lv_dropdown_get_selected(_ddPoll)];
    uint16_t timeout = TIMEOUT_VALUES[lv_dropdown_get_selected(_ddTimeout)];
    uint8_t retryCount = (uint8_t)RETRY_COUNT_VALUES[lv_dropdown_get_selected(_ddRetryCount)];
    uint16_t retryDelay = RETRY_DELAY_VALUES[lv_dropdown_get_selected(_ddRetryDelay)];
    if (endpoint != s.endpoint || apiKey != s.apiKey ||
        poll != s.pollIntervalSec || timeout != s.timeoutSec ||
        retryCount != s.retryCount || retryDelay != s.retryDelaySec) {
        _settings->setApi(endpoint, apiKey, poll, timeout, retryCount, retryDelay);
        _result.apiChanged = true;
    }

    String themeName = lv_dropdown_get_selected(_ddTheme) == 1 ? "light" : "dark";
    uint8_t brightness = (uint8_t)lv_slider_get_value(_slBrightness);
    uint16_t sleepMin = SLEEP_VALUES[lv_dropdown_get_selected(_ddSleep)];
    bool tempF = lv_obj_has_state(_swTempF, LV_STATE_CHECKED);
    if (themeName != s.theme || brightness != s.brightness ||
        sleepMin != s.sleepTimeoutMin || tempF != s.tempFahrenheit) {
        _settings->setGeneral(themeName, brightness, sleepMin, tempF, s.usAqi);
        _result.generalChanged = true;
    }

    // Rebuild layout JSON from the default, keeping only enabled widgets.
    JsonDocument defaults;
    deserializeJson(defaults, SettingsManager::defaultLayoutJson());
    JsonDocument out;
    JsonArray outLayout = out["layout"].to<JsonArray>();
    for (JsonObject item : defaults["layout"].as<JsonArray>()) {
        const char* type = item["type"] | "";
        const char* metric = item["metric"] | "";
        for (int i = 0; i < WIDGET_COUNT; i++) {
            const WidgetEntry& entry = WIDGET_CATALOG[i];
            bool matches = strcmp(type, entry.type) == 0 &&
                           (strcmp(entry.type, "metric") != 0 || strcmp(metric, entry.id) == 0);
            if (matches) {
                if (lv_obj_has_state(_swWidgets[i], LV_STATE_CHECKED)) outLayout.add(item);
                break;
            }
        }
    }
    String layoutJson;
    serializeJson(out, layoutJson);
    if (layoutJson != s.layoutJson) {
        _settings->setLayoutJson(layoutJson);
        _result.layoutChanged = true;
    }
}

void SettingsScreen::close() {
    save();
    auto cb = _closeCb;
    Result res = _result;
    _screen = nullptr;
    // The callback must load the next screen with LVGL's auto-delete
    // (lv_screen_load_anim(..., true)) so this screen is freed only after
    // the transition. Deleting it here, while it is still active/animating,
    // is a use-after-free that hard-hangs the render loop.
    if (cb) cb(res);
}
