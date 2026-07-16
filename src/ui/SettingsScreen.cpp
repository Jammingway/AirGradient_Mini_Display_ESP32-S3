#include "SettingsScreen.h"
#include <Arduino.h>  // ESP.restart()
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
    {"updated", "updated", "System info"},
};

static const char* POLL_OPTIONS = "15 s\n30 s\n1 min\n3 min\n5 min\n15 min";
static const uint16_t POLL_VALUES[] = {15, 30, 60, 180, 300, 900};
static constexpr uint16_t POLL_DEFAULT_INDEX = 4;  // 5 min
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

// ---------- terminal skin helpers --------------------------------------------
// The settings menu matches the boot console: green-on-black, monospace
// unscii type, with a soft green "glow" (shadow) on interactive elements.
// NOTE: unscii has no LVGL symbol glyphs — anything that renders symbols
// (keyboard, eye toggle) keeps a Montserrat font and is only recolored.

static lv_color_t termGreen(const ThemePalette& p) { return p.terminalGreen; }
static lv_color_t termDim(const ThemePalette& p) {
    return lv_color_mix(p.terminalGreen, lv_color_black(), 140);
}
static lv_color_t termField() { return lv_color_hex(0x081408); }

static void glow(lv_obj_t* obj, const ThemePalette& p, lv_style_selector_t sel = 0) {
    lv_obj_set_style_shadow_color(obj, termGreen(p), sel);
    lv_obj_set_style_shadow_width(obj, 14, sel);
    lv_obj_set_style_shadow_spread(obj, 1, sel);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, sel);
}

static void termButton(lv_obj_t* btn, const ThemePalette& p) {
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, termDim(p), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_color(btn, termGreen(p), LV_STATE_PRESSED);
    glow(btn, p, LV_STATE_PRESSED);
}

void SettingsScreen::open(SettingsManager& settings, const ThemeManager& theme, CloseCallback cb) {
    _settings = &settings;
    _theme = &theme;
    _closeCb = std::move(cb);
    _result = {};
    const ThemePalette& p = theme.palette();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Header: back button + title, console style
    lv_obj_t* back = lv_button_create(_screen);
    lv_obj_set_size(back, 96, 40);
    termButton(back, p);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_obj_t* backLbl = lv_label_create(back);
    lv_label_set_text(backLbl, "< SAVE");
    lv_obj_set_style_text_font(backLbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(backLbl, termGreen(p), 0);
    lv_obj_center(backLbl);
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        static_cast<SettingsScreen*>(lv_event_get_user_data(e))->close();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* title = lv_label_create(_screen);
    lv_obj_set_style_text_font(title, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(title, termGreen(p), 0);
    lv_label_set_text(title, "[ SETTINGS ]");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Tabs
    lv_obj_t* tabs = lv_tabview_create(_screen);
    lv_tabview_set_tab_bar_size(tabs, 44);
    lv_obj_set_pos(tabs, 0, 56);
    lv_obj_set_size(tabs, LCD_H_RES, LCD_V_RES - 56);
    lv_obj_set_style_bg_opa(tabs, LV_OPA_TRANSP, 0);
    lv_obj_t* bar = lv_tabview_get_tab_bar(tabs);
    lv_obj_set_style_bg_color(bar, lv_color_black(), 0);

    buildNetworkTab(lv_tabview_add_tab(tabs, "NETWORK"));
    buildApiTab(lv_tabview_add_tab(tabs, "ENDPOINT"));
    buildGeneralTab(lv_tabview_add_tab(tabs, "GENERAL"));
    buildDashboardTab(lv_tabview_add_tab(tabs, "DASHBOARD"));

    // Tab bar buttons: dim green idle, bright green + underline when active.
    uint32_t tabCount = lv_obj_get_child_count(bar);
    for (uint32_t i = 0; i < tabCount; i++) {
        lv_obj_t* btn = lv_obj_get_child(bar, i);
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_text_font(btn, &lv_font_unscii_16, 0);
        lv_obj_set_style_text_color(btn, termDim(p), 0);
        lv_obj_set_style_text_color(btn, termGreen(p), LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, termGreen(p), LV_STATE_CHECKED);
    }

    // Shared keyboard, hidden until a textarea takes focus.
    // Keeps its symbol-capable default font; only recolored to match.
    _keyboard = lv_keyboard_create(_screen);
    lv_obj_set_size(_keyboard, LCD_H_RES, 200);
    lv_obj_set_style_bg_color(_keyboard, lv_color_black(), 0);
    lv_obj_set_style_bg_color(_keyboard, termField(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(_keyboard, termGreen(p), LV_PART_ITEMS);
    lv_obj_set_style_border_width(_keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(_keyboard, termDim(p), LV_PART_ITEMS);
    lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
    auto hideKeyboard = [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        lv_obj_add_flag(self->_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(self->_keyboard, nullptr);
    };
    lv_obj_add_event_cb(_keyboard, hideKeyboard, LV_EVENT_READY, this);
    lv_obj_add_event_cb(_keyboard, hideKeyboard, LV_EVENT_CANCEL, this);

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
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(lbl, termDim(p), 0);
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
    // set_text leaves the cursor (and scroll) at the end, hiding the start
    // of long values like URLs; park it at the front so "http://" shows.
    lv_textarea_set_cursor_pos(ta, 0);
    // unscii has no bullet glyph; use a plain asterisk for password dots.
    lv_textarea_set_password_bullet(ta, "*");
    lv_textarea_set_password_mode(ta, password);
    lv_obj_set_size(ta, 440, 56);
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_pad_ver(ta, 16, 0);
    lv_obj_set_style_radius(ta, 4, 0);
    lv_obj_set_style_bg_color(ta, termField(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(ta, termGreen(p), 0);
    lv_obj_set_style_border_color(ta, termDim(p), 0);
    lv_obj_set_style_border_color(ta, termGreen(p), LV_STATE_FOCUSED);
    glow(ta, p, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(ta, termGreen(p), LV_PART_CURSOR | LV_STATE_FOCUSED);
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
    lv_dropdown_set_symbol(dd, "v");  // ASCII stand-in; unscii has no arrow glyph
    lv_obj_set_width(dd, 220);
    lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_radius(dd, 4, 0);
    lv_obj_set_style_bg_color(dd, termField(), 0);
    lv_obj_set_style_text_font(dd, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(dd, termGreen(p), 0);
    lv_obj_set_style_border_color(dd, termDim(p), 0);
    lv_obj_set_style_border_color(dd, termGreen(p), LV_STATE_FOCUSED);
    glow(dd, p, LV_STATE_FOCUSED);

    lv_obj_t* list = lv_dropdown_get_list(dd);
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_border_color(list, termGreen(p), 0);
    lv_obj_set_style_text_font(list, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(list, termGreen(p), 0);
    lv_obj_set_style_bg_color(list, termDim(p), LV_PART_SELECTED | LV_STATE_CHECKED);
    return dd;
}

lv_obj_t* SettingsScreen::makeSwitchRow(lv_obj_t* parent, const char* label, bool state) {
    const ThemePalette& p = _theme->palette();
    lv_obj_t* row = makeRowBase(parent, label, p);

    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(sw, termField(), 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_set_style_border_color(sw, termDim(p), 0);
    lv_obj_set_style_bg_color(sw, termDim(p), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, termGreen(p), LV_PART_KNOB);
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

    // Fallback network is opt-in: the toggle reveals its two rows. Default
    // on only if a fallback SSID is already stored.
    bool haveFallback = s.ssid2.length() > 0;
    _swFallback = makeSwitchRow(tab, "Fallback Network (Optional)", haveFallback);
    _taSsid2 = makeTextRow(tab, "Fallback SSID", s.ssid2);
    _taPass2 = makeTextRow(tab, "Fallback password", s.pass2, true);
    _rowSsid2 = lv_obj_get_parent(_taSsid2);
    _rowPass2 = lv_obj_get_parent(_taPass2);
    if (!haveFallback) {
        lv_obj_add_flag(_rowSsid2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_rowPass2, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_event_cb(_swFallback, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        bool on = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
        if (on) {
            lv_obj_remove_flag(self->_rowSsid2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(self->_rowPass2, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(self->_rowSsid2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(self->_rowPass2, LV_OBJ_FLAG_HIDDEN);
        }
    }, LV_EVENT_VALUE_CHANGED, this);
}

void SettingsScreen::buildApiTab(lv_obj_t* tab) {
    const AppSettings& s = _settings->get();
    const ThemePalette& p = _theme->palette();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);

    // Base URL or IP of the AirGradient sensor's local server; the firmware
    // appends /measures/current (a full cloud URL also works).
    _taEndpoint = makeTextRow(tab, "Base URL or IP", s.endpoint);

    // API token (cloud only): wide field + eye button to reveal the value.
    _taApiKey = makeTextRow(tab, "Token (cloud only)", s.apiKey, true);
    lv_obj_t* row = lv_obj_get_parent(_taApiKey);
    lv_obj_set_width(_taApiKey, 440 - 64);
    lv_obj_align(_taApiKey, LV_ALIGN_RIGHT_MID, -68, 0);

    lv_obj_t* eye = lv_button_create(row);
    lv_obj_set_size(eye, 56, 56);
    termButton(eye, p);
    lv_obj_align(eye, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_t* eyeLbl = lv_label_create(eye);
    lv_label_set_text(eyeLbl, LV_SYMBOL_EYE_OPEN);  // needs symbol font
    lv_obj_set_style_text_font(eyeLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(eyeLbl, termGreen(p), 0);
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
                              indexOfValue(POLL_VALUES, 6, s.pollIntervalSec,
                                           POLL_DEFAULT_INDEX));
    _ddTimeout = makeDropdownRow(tab, "Request timeout", TIMEOUT_OPTIONS,
                                 indexOfValue(TIMEOUT_VALUES, 3, s.timeoutSec));
    _ddRetryCount = makeDropdownRow(tab, "Attempts per poll", RETRY_COUNT_OPTIONS,
                                    indexOfValue(RETRY_COUNT_VALUES, 5, s.retryCount,
                                                 RETRY_COUNT_DEFAULT_INDEX));
    _ddRetryDelay = makeDropdownRow(tab, "Retry delay", RETRY_DELAY_OPTIONS,
                                    indexOfValue(RETRY_DELAY_VALUES, 4, s.retryDelaySec,
                                                 RETRY_DELAY_DEFAULT_INDEX));
}

void SettingsScreen::buildGeneralTab(lv_obj_t* tab) {
    const AppSettings& s = _settings->get();
    const ThemePalette& p = _theme->palette();
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);

    // Blank falls back to the name the sensor reports (model/serial).
    _taDeviceName = makeTextRow(tab, "Sensor name", s.deviceName);
    lv_textarea_set_max_length(_taDeviceName, 24);

    _ddTheme = makeDropdownRow(tab, "Theme", "Dark\nLight", s.theme == "light" ? 1 : 0);

    lv_obj_t* row = makeRowBase(tab, "Brightness", p);
    _slBrightness = lv_slider_create(row);
    lv_slider_set_range(_slBrightness, 20, 100);
    lv_slider_set_value(_slBrightness, s.brightness, LV_ANIM_OFF);
    lv_obj_set_size(_slBrightness, 320, 12);
    lv_obj_align(_slBrightness, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_bg_color(_slBrightness, termField(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_slBrightness, termDim(p), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_slBrightness, termGreen(p), LV_PART_KNOB);

    // Live readout just left of the bar. Fixed width + right-aligned so the
    // slider doesn't shift when the value crosses 100%.
    _lblBrightness = lv_label_create(row);
    lv_obj_set_style_text_font(_lblBrightness, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(_lblBrightness, termGreen(p), 0);
    lv_obj_set_width(_lblBrightness, 52);
    lv_obj_set_style_text_align(_lblBrightness, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(_lblBrightness, LV_ALIGN_RIGHT_MID, -(16 + 320 + 10), 0);
    lv_label_set_text_fmt(_lblBrightness, "%d%%", (int)s.brightness);

    lv_obj_add_event_cb(_slBrightness, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        lv_label_set_text_fmt(self->_lblBrightness, "%d%%",
                              (int)lv_slider_get_value(self->_slBrightness));
    }, LV_EVENT_VALUE_CHANGED, this);

    _ddSleep = makeDropdownRow(tab, "Display Sleep", SLEEP_OPTIONS,
                               indexOfValue(SLEEP_VALUES, 6, s.sleepTimeoutMin));
    _swTempF = makeSwitchRow(tab, "Temperature in \xC2\xB0""F", s.tempFahrenheit);
    _swDisableSplash = makeSwitchRow(tab, "Disable splash screen", s.disableSplash);
    _swDebug = makeSwitchRow(tab, "Enable debug overlay", s.debug);

    // The physical RESET button is unreliable on this board: GPIO0 doubles as
    // LCD_PIN_D6 (see board_pins.h), so a hardware EN reset while the display
    // is actively driving that line can sample it low and drop into the ROM's
    // UART-download bootloader instead of rebooting the app (confirmed on
    // hardware — 100% reproducible, independent of power source). A software
    // restart goes through a different reset path, evidenced by every
    // crash-triggered reboot this project has seen landing back in the app,
    // never in download mode.
    lv_obj_t* restartBtn = lv_button_create(tab);
    lv_obj_set_size(restartBtn, LV_PCT(100), 48);
    termButton(restartBtn, p);
    lv_obj_t* restartLbl = lv_label_create(restartBtn);
    lv_label_set_text(restartLbl, "RESTART DEVICE");
    lv_obj_set_style_text_font(restartLbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(restartLbl, termGreen(p), 0);
    lv_obj_center(restartLbl);
    lv_obj_add_event_cb(restartBtn, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        self->save();   // persist pending edits first, same as "< SAVE"
        ESP.restart();  // never returns
    }, LV_EVENT_CLICKED, this);
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
    termButton(resetBtn, p);
    lv_obj_t* resetLbl = lv_label_create(resetBtn);
    lv_label_set_text(resetLbl, "RESET LAYOUT TO DEFAULT");
    lv_obj_set_style_text_font(resetLbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(resetLbl, termGreen(p), 0);
    lv_obj_center(resetLbl);
    lv_obj_add_event_cb(resetBtn, [](lv_event_t* e) {
        auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        for (int i = 0; i < WIDGET_COUNT; i++) {
            lv_obj_add_state(self->_swWidgets[i], LV_STATE_CHECKED);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* factoryBtn = lv_button_create(tab);
    lv_obj_set_size(factoryBtn, LV_PCT(100), 48);
    termButton(factoryBtn, p);
    lv_obj_set_style_border_color(factoryBtn, lv_color_hex(0xD64545), 0);
    lv_obj_t* factoryLbl = lv_label_create(factoryBtn);
    lv_label_set_text(factoryLbl, "FACTORY RESET (ERASE ALL SETTINGS)");
    lv_obj_set_style_text_font(factoryLbl, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(factoryLbl, lv_color_hex(0xD64545), 0);
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
    // A disabled fallback toggle clears the fallback credentials entirely.
    bool fallbackOn = lv_obj_has_state(_swFallback, LV_STATE_CHECKED);
    String ssid2 = fallbackOn ? String(lv_textarea_get_text(_taSsid2)) : String("");
    String pass2 = fallbackOn ? String(lv_textarea_get_text(_taPass2)) : String("");
    if (ssid1 != s.ssid1 || pass1 != s.pass1 || ssid2 != s.ssid2 || pass2 != s.pass2) {
        _settings->setNetwork(ssid1, pass1, ssid2, pass2);
        _result.networkChanged = true;
    }

    String endpoint = lv_textarea_get_text(_taEndpoint);
    String apiKey = lv_textarea_get_text(_taApiKey);  // may be blank (local endpoint)
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

    String deviceName = lv_textarea_get_text(_taDeviceName);
    deviceName.trim();
    if (deviceName != s.deviceName) {
        _settings->setDeviceName(deviceName);
        _result.generalChanged = true;
    }

    String themeName = lv_dropdown_get_selected(_ddTheme) == 1 ? "light" : "dark";
    uint8_t brightness = (uint8_t)lv_slider_get_value(_slBrightness);
    uint16_t sleepMin = SLEEP_VALUES[lv_dropdown_get_selected(_ddSleep)];
    bool tempF = lv_obj_has_state(_swTempF, LV_STATE_CHECKED);
    bool noSplash = lv_obj_has_state(_swDisableSplash, LV_STATE_CHECKED);
    bool debug = lv_obj_has_state(_swDebug, LV_STATE_CHECKED);
    if (themeName != s.theme || brightness != s.brightness ||
        sleepMin != s.sleepTimeoutMin || tempF != s.tempFahrenheit ||
        noSplash != s.disableSplash || debug != s.debug) {
        _settings->setGeneral(themeName, brightness, sleepMin, tempF, s.usAqi, noSplash, debug);
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
