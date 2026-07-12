/**
 * @file ThemeManager.h
 * Palette definitions. Dark is the default; severity colors are shared
 * across themes so air-quality color language stays consistent.
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>

enum class Severity : uint8_t { Good, Moderate, Poor, Bad, Severe, Neutral };

struct ThemePalette {
    lv_color_t bg;
    lv_color_t card;
    lv_color_t cardBorder;
    lv_color_t text;
    lv_color_t textMuted;
    lv_color_t accent;
    lv_color_t terminalGreen;
    bool dark;
};

class ThemeManager {
public:
    void apply(const String& name);  // "dark" | "light"
    const ThemePalette& palette() const { return _p; }
    lv_color_t severityColor(Severity s) const;

private:
    ThemePalette _p{};
};
