/**
 * @file InfoWidget.h
 * Small status cards: WiFi link state and the "System" panel (data freshness,
 * display uptime, sensor uptime). Text is provided by the dashboard, which
 * owns the context.
 */
#pragma once
#include "Widget.h"

class InfoWidget : public Widget {
public:
    enum class Kind : uint8_t { Wifi, Updated };

    // Detail-row slots. Each card fixes its own labels at create() time; the
    // dashboard fills the values by slot. Wifi uses the Wifi* entries,
    // Updated the Sys* ones — the overlapping values are intentional.
    enum Row : uint8_t {
        WifiIp = 0,
        WifiSensor = 1,

        SysLastPolled = 0,
        SysUptime = 1,
        SysSensorUptime = 2,
    };

    explicit InfoWidget(Kind kind) : _kind(kind) {}

    void create(lv_obj_t* parent, const ThemeManager& theme) override;
    void update(const AirGradientReading& r, const AppSettings& s,
                const ThemeManager& theme) override;
    InfoWidget* asInfo() override { return this; }

    // Headline text pushed by the dashboard each refresh tick. On the WiFi
    // card this is the big link-status line; on the System card it routes to
    // the "Last polled:" row, which serves the same purpose.
    void setText(const String& value, bool alert, const ThemeManager& theme);

    // Fills one detail row; an empty value shows a placeholder.
    void setRow(Row row, const String& value);

    Kind kind() const { return _kind; }

private:
    static constexpr int MAX_ROWS = 3;

    // Appends a right-aligned "label / value" row and returns the value label.
    lv_obj_t* addRow(const ThemePalette& p, const char* label);

    Kind _kind;
    lv_obj_t* _nameLbl = nullptr;
    lv_obj_t* _valueLbl = nullptr;  // WiFi card only
    lv_obj_t* _icon = nullptr;
    lv_obj_t* _rowVal[MAX_ROWS] = {};
};
