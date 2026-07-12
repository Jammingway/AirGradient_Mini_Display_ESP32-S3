/**
 * @file Dashboard.h
 * Configuration-driven dashboard. The persisted layout JSON decides which
 * widgets exist and where they sit on the logical grid; this class only
 * translates that description into LVGL objects.
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <functional>
#include <vector>
#include <memory>
#include "../widgets/Widget.h"
#include "../widgets/InfoWidget.h"

class ThemeManager;
class SettingsManager;

class Dashboard {
public:
    using ActionCallback = std::function<void()>;

    void create(const ThemeManager& theme, const SettingsManager& settings);
    // deletePrev: let LVGL delete the previously active screen once the
    // transition completes (required when leaving the settings screen).
    void load(bool deletePrev = false);

    // Rebuilds all widgets from the current settings layout JSON.
    void rebuild(const ThemeManager& theme, const SettingsManager& settings);

    void updateReading(const AirGradientReading& r, const AppSettings& s,
                       const ThemeManager& theme);
    void updateStatus(const String& wifiText, bool wifiAlert,
                      const String& updatedText, bool updatedAlert,
                      const String& headline, const ThemeManager& theme);

    void onRefresh(ActionCallback cb) { _refreshCb = std::move(cb); }
    void onSettings(ActionCallback cb) { _settingsCb = std::move(cb); }

    lv_obj_t* screen() { return _screen; }

private:
    void buildTopBar(const ThemeManager& theme);
    void buildWidgets(const ThemeManager& theme, const SettingsManager& settings);

    void onUpdatedCardClicked();

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _grid = nullptr;
    lv_obj_t* _titleLbl = nullptr;
    lv_obj_t* _headlineLbl = nullptr;
    const ThemeManager* _theme = nullptr;

    std::vector<std::unique_ptr<Widget>> _widgets;
    ActionCallback _refreshCb;
    ActionCallback _settingsCb;

    static constexpr int TOP_BAR_H = 56;
    static constexpr int GAP = 12;
};
