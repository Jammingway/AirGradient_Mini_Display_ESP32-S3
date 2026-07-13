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
#include "../history/HistoryManager.h"

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

    // Debug readout pinned bottom-left; empty string hides it.
    void setDebugLine(const String& text);

    // Source of trend data for the tap-to-open charts.
    void setHistory(HistoryManager* h) { _history = h; }

    lv_obj_t* screen() { return _screen; }

private:
    void buildTopBar(const ThemeManager& theme);
    void buildWidgets(const ThemeManager& theme, const SettingsManager& settings);
    void flowLayout();

    void onUpdatedCardClicked();

    // Full-screen trend chart overlay.
    void buildChartOverlay();
    void showChart(int metric);
    void hideChart();
    void refreshChart();
    void highlightDurationButtons();
    static int metricEnumForId(const char* id);

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _grid = nullptr;
    lv_obj_t* _titleLbl = nullptr;
    lv_obj_t* _headlineLbl = nullptr;
    lv_obj_t* _debugLbl = nullptr;
    const ThemeManager* _theme = nullptr;

    std::vector<std::unique_ptr<Widget>> _widgets;
    ActionCallback _refreshCb;
    ActionCallback _settingsCb;

    // Trend chart overlay state.
    HistoryManager* _history = nullptr;
    bool _tempF = false;
    lv_obj_t* _chartOverlay = nullptr;
    lv_obj_t* _chart = nullptr;
    lv_chart_series_t* _chartSeries = nullptr;
    lv_obj_t* _chartTitle = nullptr;
    lv_obj_t* _chartStats = nullptr;
    lv_obj_t* _durBtns[4] = {};
    int _chartMetric = -1;
    uint16_t _chartWindowMin = 360;  // 6h default

    static constexpr int TOP_BAR_H = 56;
    static constexpr int GAP = 12;
};
