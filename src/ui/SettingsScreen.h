/**
 * @file SettingsScreen.h
 * Touch settings UI: Network (two SSIDs, second optional), API, General,
 * Dashboard tabs with a shared on-screen keyboard. Values persist through
 * SettingsManager when the user leaves via the back button.
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <functional>

class SettingsManager;
class ThemeManager;

class SettingsScreen {
public:
    struct Result {
        bool networkChanged = false;
        bool apiChanged = false;
        bool generalChanged = false;
        bool layoutChanged = false;
        bool factoryReset = false;
    };
    // The callback MUST load another screen; this screen deletes itself
    // right after the callback returns.
    using CloseCallback = std::function<void(const Result&)>;

    void open(SettingsManager& settings, const ThemeManager& theme, CloseCallback cb);
    bool isOpen() const { return _screen != nullptr; }

private:
    void buildNetworkTab(lv_obj_t* tab);
    void buildApiTab(lv_obj_t* tab);
    void buildGeneralTab(lv_obj_t* tab);
    void buildDashboardTab(lv_obj_t* tab);

    lv_obj_t* makeTextRow(lv_obj_t* parent, const char* label, const String& value,
                          bool password = false);
    lv_obj_t* makeDropdownRow(lv_obj_t* parent, const char* label, const char* options,
                              uint16_t selected);
    lv_obj_t* makeSwitchRow(lv_obj_t* parent, const char* label, bool state);

    void attachKeyboard(lv_obj_t* textarea);
    void save();
    void close();

    SettingsManager* _settings = nullptr;
    const ThemeManager* _theme = nullptr;
    CloseCallback _closeCb;
    Result _result;

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _keyboard = nullptr;

    // Network
    lv_obj_t* _taSsid1 = nullptr;
    lv_obj_t* _taPass1 = nullptr;
    lv_obj_t* _taSsid2 = nullptr;
    lv_obj_t* _taPass2 = nullptr;
    // API
    lv_obj_t* _taEndpoint = nullptr;
    lv_obj_t* _taApiKey = nullptr;
    lv_obj_t* _ddPoll = nullptr;
    lv_obj_t* _ddTimeout = nullptr;
    // General
    lv_obj_t* _ddTheme = nullptr;
    lv_obj_t* _slBrightness = nullptr;
    lv_obj_t* _ddSleep = nullptr;
    lv_obj_t* _swTempF = nullptr;
    // Dashboard
    static constexpr int WIDGET_COUNT = 9;
    lv_obj_t* _swWidgets[WIDGET_COUNT] = {};
};
