/**
 * @file Widget.h
 * Dashboard widget interface. Widgets are placed on a logical grid
 * (columns x rows) described by the persisted layout JSON; the dashboard
 * translates grid cells to pixels so widgets never hardcode positions.
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include "../models/AirGradientReading.h"
#include "../themes/ThemeManager.h"
#include "../settings/SettingsManager.h"

struct GridCell {
    uint8_t x = 0, y = 0, w = 1, h = 1;
};

class InfoWidget;

class Widget {
public:
    virtual ~Widget() = default;

    virtual void create(lv_obj_t* parent, const ThemeManager& theme) = 0;
    virtual void update(const AirGradientReading& r, const AppSettings& s,
                        const ThemeManager& theme) = 0;

    // RTTI-free downcast (Arduino builds use -fno-rtti).
    virtual InfoWidget* asInfo() { return nullptr; }

    void setCell(const GridCell& c) { _cell = c; }
    const GridCell& cell() const { return _cell; }
    lv_obj_t* root() { return _root; }

    // Places the widget's root object into pixel space.
    void place(int originX, int originY, int cellW, int cellH, int gap);

protected:
    // Standard card container used by all widgets.
    lv_obj_t* makeCard(lv_obj_t* parent, const ThemePalette& p);

    lv_obj_t* _root = nullptr;
    GridCell _cell;
};
