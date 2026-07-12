/**
 * @file MetricWidget.h
 * One card per metric: name, big value, unit, severity accent bar.
 * All metric definitions live in a single descriptor table so adding a
 * metric never touches the renderer.
 */
#pragma once
#include "Widget.h"

struct MetricDescriptor {
    const char* id;        // layout JSON key, e.g. "pm25"
    const char* label;
    const char* unit;      // may be overridden at runtime (temp)
    uint8_t decimals;
    float (*get)(const AirGradientReading&);
    Severity (*severity)(float);
};

class MetricWidget : public Widget {
public:
    explicit MetricWidget(const MetricDescriptor* desc) : _desc(desc) {}

    void create(lv_obj_t* parent, const ThemeManager& theme) override;
    void update(const AirGradientReading& r, const AppSettings& s,
                const ThemeManager& theme) override;

    static const MetricDescriptor* find(const char* id);

private:
    const MetricDescriptor* _desc;
    lv_obj_t* _nameLbl = nullptr;
    lv_obj_t* _valueLbl = nullptr;
    lv_obj_t* _unitLbl = nullptr;
    lv_obj_t* _bar = nullptr;
};
