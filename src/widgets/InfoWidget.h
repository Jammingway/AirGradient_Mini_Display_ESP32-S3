/**
 * @file InfoWidget.h
 * Small status cards: WiFi link state and data freshness ("last updated").
 * Text is provided by the dashboard, which owns the context.
 */
#pragma once
#include "Widget.h"

class InfoWidget : public Widget {
public:
    enum class Kind : uint8_t { Wifi, Updated };

    explicit InfoWidget(Kind kind) : _kind(kind) {}

    void create(lv_obj_t* parent, const ThemeManager& theme) override;
    void update(const AirGradientReading& r, const AppSettings& s,
                const ThemeManager& theme) override;
    InfoWidget* asInfo() override { return this; }

    // Live status text pushed by the dashboard each refresh tick.
    void setText(const String& value, bool alert, const ThemeManager& theme);

    Kind kind() const { return _kind; }

private:
    Kind _kind;
    lv_obj_t* _nameLbl = nullptr;
    lv_obj_t* _valueLbl = nullptr;
    lv_obj_t* _icon = nullptr;
};
