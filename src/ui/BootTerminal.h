/**
 * @file BootTerminal.h
 * Retro boot console: green "> " prompt lines with a blinking "_" cursor,
 * mirroring boot/network status. Tapping the screen fires a callback
 * (used to open setup when unconfigured or offline).
 */
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <functional>

class ThemeManager;

class BootTerminal {
public:
    using TapCallback = std::function<void()>;

    void create(const ThemeManager& theme);
    // deletePrev: let LVGL delete the previously active screen once the
    // transition completes (required when leaving the settings screen).
    void load(bool deletePrev = false);

    // Finalize the current line and start a new "> msg" line.
    void pushLine(const String& msg);
    // Replace the text of the current line (progress updates).
    void setStatus(const String& msg);
    // Show/hide the "TAP TO CONFIGURE" hint.
    void showConfigHint(bool show);

    void onTap(TapCallback cb) { _tap = std::move(cb); }
    lv_obj_t* screen() { return _screen; }

private:
    void refresh();
    static void blinkTimerCb(lv_timer_t* t);

    static constexpr int MAX_LINES = 12;

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _lineLabels[MAX_LINES] = {};
    lv_obj_t* _cursor = nullptr;
    lv_obj_t* _hint = nullptr;
    lv_timer_t* _blinkTimer = nullptr;

    String _lines[MAX_LINES];
    int _count = 0;
    TapCallback _tap;
};
